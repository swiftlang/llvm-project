; Verify loop-trap-analysis round-trips through the textual pass pipeline
; (exercises LoopTrapAnalysisPass::printPipeline). Checks the full output.
; RUN: opt -passes='loop-trap-analysis' -print-pipeline-passes -disable-output %s \
; RUN:   | FileCheck %s --match-full-lines

; CHECK: function(loop-trap-analysis),verify

define void @f() {
entry:
  ret void
}
