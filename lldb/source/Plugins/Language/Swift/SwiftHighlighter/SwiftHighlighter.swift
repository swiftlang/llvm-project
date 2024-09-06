import CxxStdlib
import SwiftHighlighterSupport
import SwiftParser
import SwiftSyntax

private extension String {
  /// Helper function to extract ranges expressed in UTF8 offsets from a string.
  subscript(_ range: Range<AbsolutePosition>) -> Substring.UTF8View {
    let start = index(startIndex, offsetBy: range.lowerBound.utf8Offset)
    let end = index(startIndex, offsetBy: range.upperBound.utf8Offset)
    return self.utf8[start..<end]
  }
}

private extension TriviaPiece {
  var isComment: Bool {
    return switch self {
    case .lineComment, .blockComment, .docLineComment, .docBlockComment:
      true
    default:
      false
    }
  }
}

enum AnsiColor {
  enum Bright {
    static let red = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.bright.red}"), true))
    static let green = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.bright.green}"), true))
    static let yellow = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.bright.yellow}"), true))
    static let blue = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.bright.blue}"), true))
    static let purple = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.bright.purple}"), true))
    static let cyan = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.bright.cyan}"), true))
  }
  static let red = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.red}"), true))
  static let green = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.green}"), true))
  static let yellow = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.yellow}"), true))
  static let blue = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.blue}"), true))
  static let purple = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.purple}"), true))
  static let cyan = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.fg.cyan}"), true))
  static let normal = String(ansi.FormatAnsiTerminalCodes(std.string("${ansi.normal}"), true))
}

/// The different syntactic elements that can be highlighted.
enum HighlightStyle {
  case comment
  case stringLiteral
  case numberLiteral
  case keyword
  case attribute
  case typeDeclaration
  case otherDeclaration
  case typeIdentifier
  case function
  case functionLabel

  func getColor() -> String {
    return switch self {
    case .comment: AnsiColor.purple
    case .stringLiteral: AnsiColor.green
    case .numberLiteral: AnsiColor.yellow
    case .keyword: AnsiColor.Bright.red
    case .attribute: AnsiColor.red
    case .typeDeclaration: AnsiColor.Bright.green
    case .otherDeclaration: AnsiColor.normal
    case .typeIdentifier: AnsiColor.green
    case .function: AnsiColor.cyan
    case .functionLabel: AnsiColor.cyan
    }
  }
}

struct HighlightMarker: Comparable {
  enum Priority: Comparable {
    case low
    case medium
    case high
  }

  // The range inside the source code that should be highlighted.
  let range: Range<AbsolutePosition>
  // The style that this highlight should adopt.
  let style: HighlightStyle
  // The priority of this marker. Multiple highlight markers can be parsed for the same
  // range, the priority is used to pick which highlight to keep.
  let priority: Priority

  init(range: Range<AbsolutePosition>, style: HighlightStyle, priority: Priority) {
    self.range = range
    self.style = style
    self.priority = priority
  }

  func produceHighlight(using string: String) -> String {
    return "\(style.getColor())\(String(string[range])!)\(AnsiColor.normal)"
  }

  static func < (lhs: HighlightMarker, rhs: HighlightMarker) -> Bool {
    assert(!lhs.range.overlaps(rhs.range), "Ranges overlap!")
    return lhs.range.lowerBound < rhs.range.upperBound
  }
}

class SwiftSyntaxHighlighter: SyntaxVisitor {
  private let source: String

  /// The set of markers parsed from the source.
  private(set) var highlightMarkers = [HighlightMarker]()

  /// The range that should be highlighted. The source might have more text than what will be highlighted,
  /// but be included to provide better context for the SyntaxVisitor. With the range, SwiftSyntaxHighlighter
  /// can avoid adding highlight markers to text that won't be displayed, as well as skipping over SyntaxNodes
  /// that are outside of the range of what will be highlighted.
  private let highlightRange: Range<AbsolutePosition>

  private init(sourceString: String, highlightRange: Range<AbsolutePosition>) {
    self.source = sourceString
    let sourceFile: SourceFileSyntax = Parser.parse(source: sourceString)
    self.highlightRange = highlightRange
    super.init(viewMode: .all)
    walk(sourceFile)
  }

  private func mark(range: Range<AbsolutePosition>, style: HighlightStyle, priority: HighlightMarker.Priority = .medium)
  {
    let newMarker = HighlightMarker(range: range, style: style, priority: priority)

    // Find if there is a marker in the same range of the one we're trying to add.
    let storedMarkerIndex = highlightMarkers.firstIndex { storedMarker in
      newMarker.range == storedMarker.range
    }

    // If there is, keep the one with higher priority.
    if let storedMarkerIndex {
      let storedMarker = highlightMarkers[storedMarkerIndex]
      assert(storedMarker.priority != newMarker.priority, "Markers with same range have same priority")
      if storedMarker.priority < newMarker.priority {
        self.highlightMarkers.remove(at: storedMarkerIndex)
      } else {
        return
      }
    }

    let overlapped = highlightMarkers.first(where: { $0.range.overlaps(newMarker.range) })
    assert(overlapped == nil, "Ranges overlap!")

    self.highlightMarkers.append(newMarker)
  }

  private func extractPositionWithoutTrivia(from syntax: SyntaxProtocol) -> Range<AbsolutePosition> {
    return syntax.positionAfterSkippingLeadingTrivia..<syntax.endPositionBeforeTrailingTrivia
  }

  private func addHighlightIfVisible(
    to node: SyntaxProtocol,
    withStyle style: HighlightStyle,
    andPriority priority: HighlightMarker.Priority = .medium
  ) {
    // If this node's range doesn't overlap with what needs to be highlighted there's nothing to do.
    guard node.range.overlaps(highlightRange) else {
      return
    }
    mark(range: extractPositionWithoutTrivia(from: node), style: style, priority: priority)
  }

  private func addHighlightIfVisible(
    toRange range: Range<AbsolutePosition>,
    with style: HighlightStyle,
    and priority: HighlightMarker.Priority = .medium
  ) {
    // If this node's range doesn't overlap with what needs to be highlighted there's nothing to do.
    guard range.overlaps(highlightRange) else {
      return
    }
    mark(range: range, style: style, priority: priority)
  }

  // Parse all the trivia pieces attached to the trivia. For highlight purposes these are comments.
  private func parse(trivia: Trivia, startingFrom startPosition: AbsolutePosition) {
    var startPosition = startPosition
    for triviaPiece in trivia.pieces {
      let endPosition = startPosition + triviaPiece.sourceLength
      if triviaPiece.isComment {
        addHighlightIfVisible(toRange: startPosition..<endPosition, with: .comment)
      }
      startPosition = endPosition
    }
  }

  override func visit(_ token: TokenSyntax) -> SyntaxVisitorContinueKind {
    guard token.range.overlaps(highlightRange) else { return .skipChildren }

    switch token.tokenKind {
    case .keyword:
      // Keywords might already be processed as identifiers ("self", "super"),
      // add them with a high priority as we want the "keyword" colorization
      // to win over the identifier one.
      addHighlightIfVisible(to: token, withStyle: .keyword, andPriority: .high)
    case .stringQuote, .stringSegment:
      addHighlightIfVisible(to: token, withStyle: .stringLiteral)
    case .integerLiteral, .floatLiteral:
      addHighlightIfVisible(to: token, withStyle: .numberLiteral)
    default:
      break
    }
    parse(trivia: token.leadingTrivia, startingFrom: token.position)
    parse(trivia: token.trailingTrivia, startingFrom: token.endPositionBeforeTrailingTrivia)

    return .visitChildren
  }

  override func visit(_ node: ClassDeclSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeDeclaration)
    return .visitChildren
  }

  override func visit(_ node: EnumDeclSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeDeclaration)
    return .visitChildren
  }

  override func visit(_ node: StructDeclSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeDeclaration)
    return .visitChildren
  }

  override func visit(_ node: ProtocolDeclSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeDeclaration)
    return .visitChildren
  }

  override func visit(_ node: ActorDeclSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeDeclaration)
    return .visitChildren
  }

  override func visit(_ node: EnumCaseElementSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .otherDeclaration)
    return .visitChildren
  }

  override func visit(_ node: AttributeSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.atSign, withStyle: .attribute, andPriority: .high)
    addHighlightIfVisible(to: node.attributeName, withStyle: .attribute, andPriority: .high)
    return .visitChildren
  }

  override func visit(_ node: IdentifierPatternSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.identifier, withStyle: .otherDeclaration)
    return .visitChildren
  }

  override func visit(_ node: FunctionDeclSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .function)
    return .visitChildren
  }

  override func visit(_ node: FunctionParameterSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.firstName, withStyle: .functionLabel)
    return .visitChildren
  }

  override func visit(_ node: IdentifierTypeSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeIdentifier)
    return .visitChildren
  }

  override func visit(_ node: MemberTypeSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    addHighlightIfVisible(to: node.name, withStyle: .typeIdentifier)
    return .visitChildren
  }

  override func visit(_ node: FunctionCallExprSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    if let memberAccess = node.calledExpression.as(MemberAccessExprSyntax.self) {
      addHighlightIfVisible(to: memberAccess.declName, withStyle: .function)
    } else if let declReference = node.calledExpression.as(DeclReferenceExprSyntax.self) {
      addHighlightIfVisible(to: declReference, withStyle: .function)
    }
    return .visitChildren
  }

  override func visit(_ node: LabeledExprSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    if let label = node.label {
      addHighlightIfVisible(to: label, withStyle: .functionLabel)
    }
    return .visitChildren
  }

  override func visit(_ node: DeclReferenceExprSyntax) -> SyntaxVisitorContinueKind {
    guard node.range.overlaps(highlightRange) else { return .skipChildren }

    // DeclReferences are a very broad category, so add highlights to them with low
    // priority, in case there are more accurate categories that can highlight them.
    addHighlightIfVisible(to: node, withStyle: .otherDeclaration, andPriority: .low)
    return .visitChildren
  }

  static func highlight(source: String, highlightRange: Range<AbsolutePosition>) -> String {
    let highlighter = SwiftSyntaxHighlighter(sourceString: source, highlightRange: highlightRange)
    let sorted = highlighter.highlightMarkers.sorted()
    var highlighted = ""
    // Because we're only highlighting whats in highlightRange, start with its lower bound.
    var lastParsedIndex = highlightRange.lowerBound
    for marker in sorted {
      if marker.range.lowerBound.utf8Offset > 0 {
        // Append any unhighlighted text before the marker.
        highlighted += String(source[lastParsedIndex..<marker.range.lowerBound])!
      }
      // Add the highlighted marker's text.
      highlighted += marker.produceHighlight(using: source)
      lastParsedIndex = marker.range.upperBound
    }
    // Add anything left after the last maerker.
    highlighted += String(source[lastParsedIndex..<highlightRange.upperBound])!
    return highlighted
  }
}

public func highlight(previousSource: String, source: String) -> String {
  let entireSource = previousSource + source
  let start = AbsolutePosition(utf8Offset: previousSource.utf8.count)
  let end = start.advanced(by: source.utf8.count)
  return SwiftSyntaxHighlighter.highlight(source: entireSource, highlightRange: start..<end)
}
