//===-- ThreadPlanStack.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_TARGET_THREADPLANSTACK_H
#define LLDB_TARGET_THREADPLANSTACK_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/lldb-private-forward.h"
#include "lldb/lldb-private.h"

namespace lldb_private {

// The ThreadPlans have a thread for use when they are asked all the ThreadPlan
// state machine questions, but they should never cache any pointers from their
// owning lldb_private::Thread.  That's because we want to be able to detach
// them from an owning thread, then reattach them by TID.
// The ThreadPlanStack holds the ThreadPlans for a given TID.  All its methods
// are private, and it should only be accessed through the owning thread.  When
// it is detached from a thread, all you can do is reattach it or delete it.
class ThreadPlanStack {
  friend class lldb_private::Thread;

public:
  ThreadPlanStack(const Thread &thread, bool make_empty = false);
  ~ThreadPlanStack() {}

  enum StackKind { ePlans, eCompletedPlans, eDiscardedPlans };

  using PlanStack = std::vector<lldb::ThreadPlanSP>;

  void DumpThreadPlans(Stream &s, lldb::DescriptionLevel desc_level,
                       bool include_internal) const;

  size_t CheckpointCompletedPlans();

  void RestoreCompletedPlanCheckpoint(size_t checkpoint);

  void DiscardCompletedPlanCheckpoint(size_t checkpoint);

  void ThreadDestroyed(Thread *thread);

  void EnableTracer(bool value, bool single_stepping);

  void SetTracer(lldb::ThreadPlanTracerSP &tracer_sp);

  void PushPlan(lldb::ThreadPlanSP new_plan_sp);

  lldb::ThreadPlanSP PopPlan();

  lldb::ThreadPlanSP DiscardPlan();

  // If the input plan is nullptr, discard all plans.  Otherwise make sure this
  // plan is in the stack, and if so discard up to and including it.
  void DiscardPlansUpToPlan(ThreadPlan *up_to_plan_ptr);

  void DiscardAllPlans();

  void DiscardConsultingMasterPlans();

  lldb::ThreadPlanSP GetCurrentPlan() const;

  lldb::ThreadPlanSP GetCompletedPlan(bool skip_private = true) const;

  lldb::ThreadPlanSP GetPlanByIndex(uint32_t plan_idx,
                                    bool skip_private = true) const;

  lldb::ValueObjectSP GetReturnValueObject(bool &is_error) const;
  
  lldb::ExpressionVariableSP GetExpressionVariable() const;

  bool AnyPlans() const;

  bool AnyCompletedPlans() const;

  bool AnyDiscardedPlans() const;

  bool IsPlanDone(ThreadPlan *plan) const;

  bool WasPlanDiscarded(ThreadPlan *plan) const;

  ThreadPlan *GetPreviousPlan(ThreadPlan *current_plan) const;

  ThreadPlan *GetInnermostExpression() const;

  void WillResume();

  bool IsTID(lldb::tid_t tid) {
    return GetTID() == tid;
  }
  lldb::tid_t GetTID();
  void SetTID(lldb::tid_t tid);

private:
  const PlanStack &GetStackOfKind(ThreadPlanStack::StackKind kind) const;

  void PrintOneStack(Stream &s, llvm::StringRef stack_name,
                     const PlanStack &stack, lldb::DescriptionLevel desc_level,
                     bool include_internal) const;

  PlanStack m_plans;           ///< The stack of plans this thread is executing.
  PlanStack m_completed_plans; ///< Plans that have been completed by this
                               /// stop.  They get deleted when the thread
                               /// resumes.
  PlanStack m_discarded_plans; ///< Plans that have been discarded by this
                               /// stop.  They get deleted when the thread
                               /// resumes.
  size_t m_completed_plan_checkpoint = 0; // Monotonically increasing token for
                                          // completed plan checkpoints.
  std::unordered_map<size_t, PlanStack> m_completed_plan_store;
  mutable std::recursive_mutex m_stack_mutex;
  
  // ThreadPlanStacks shouldn't be copied.
  ThreadPlanStack(ThreadPlanStack &rhs) = delete;
};

class ThreadPlanStackMap {
public:
  ThreadPlanStackMap(Process &process) : m_process(process) {}
  ~ThreadPlanStackMap() {}

  // Prune the map using the current_threads list.
  void Update(ThreadList &current_threads, bool delete_missing,
              bool check_for_new = true);

  void AddThread(Thread &thread) {
    lldb::tid_t tid = thread.GetID();
    std::shared_ptr<ThreadPlanStack> new_elem_sp(new ThreadPlanStack(thread));
    m_plans_sp_container.push_back(new_elem_sp);
    m_plans_list.emplace(tid, new_elem_sp.get());
  }

  bool RemoveTID(lldb::tid_t tid) {
    auto result = m_plans_list.find(tid);
    if (result == m_plans_list.end())
      return false;
    ThreadPlanStack *removed_stack = result->second;
    m_plans_list.erase(result);
    // Also remove this from the stack storage:
    PlansStore::iterator end 
        = m_plans_sp_container.end();
    PlansStore::iterator iter; 
    for (iter = m_plans_sp_container.begin();
         iter != end; iter++) {
      if ((*iter)->IsTID(tid)) {
        break;
      } 
    }
    if (iter == end)
      return false;

    // Finally, tell the stack its thread has been destroyed:
    removed_stack->ThreadDestroyed(nullptr);
    // And THEN remove it from the container so it goes away.
    m_plans_sp_container.erase(iter);
    return true;
  }

  ThreadPlanStack *Find(lldb::tid_t tid) {
    auto result = m_plans_list.find(tid);
    if (result == m_plans_list.end())
      return nullptr;
    else
      return result->second;
  }

  // rename to Reactivate?
  void Activate(ThreadPlanStack &stack) {
    // Remove this from the detached plan list:
    std::vector<ThreadPlanStack *>::iterator end = m_detached_plans.end();    
    
    for (std::vector<ThreadPlanStack *>::iterator iter 
             = m_detached_plans.begin(); iter != end; iter++) {
      if (*iter == &stack) {
        m_detached_plans.erase(iter);
        break;
      }
    }
    
    if (m_plans_list.find(stack.GetTID()) == m_plans_list.end())
      m_plans_list.emplace(stack.GetTID(), &stack);
    else
      m_plans_list.at(stack.GetTID()) = &stack;
  }

  void ScanForDetachedPlanStacks() {
    llvm::SmallVector<lldb::tid_t, 2> invalidated_tids;
    for (auto &pair : m_plans_list)
      if (pair.second->GetTID() != pair.first)
        invalidated_tids.push_back(pair.first);

    for (auto tid : invalidated_tids) {
      auto it = m_plans_list.find(tid);
      ThreadPlanStack *stack = it->second;
      m_plans_list.erase(it);
      m_detached_plans.push_back(stack);
    }
  }

  std::vector<ThreadPlanStack *> &GetDetachedPlanStacks() {
    return m_detached_plans;
  }

  void Clear() {
    for (auto &plan : m_plans_list)
      plan.second->ThreadDestroyed(nullptr);
    m_plans_list.clear();
  }

  // Implements Process::DumpThreadPlans
  void DumpPlans(Stream &strm, lldb::DescriptionLevel desc_level, bool internal,
                 bool ignore_boring, bool skip_unreported);

  // Implements Process::DumpThreadPlansForTID
  bool DumpPlansForTID(Stream &strm, lldb::tid_t tid,
                       lldb::DescriptionLevel desc_level, bool internal,
                       bool ignore_boring, bool skip_unreported);
                       
  bool PrunePlansForTID(lldb::tid_t tid);

private:
  Process &m_process;
  // We don't want to make copies of these ThreadPlanStacks, there needs to be
  // just one of these tracking each piece of work.  But we need to move the
  // work from "attached to a TID" state to "detached" state, which is most
  // conveniently done by having separate containers for the two states.
  // To do that w/o having to spend effort fighting with C++'s desire to invoke
  // copy constructors, we keep a store for the real object, and then keep
  // pointers to the objects in the two containers.  The store actually holds
  // shared_ptrs to the objects, so that the pointers in the containers will stay
  // valid.
  
  // Use a shared pointer rather than unique_ptr here to prevent C++ from trying
  // to use a copy constructor on the stored type.
  using PlansStore = std::vector<std::shared_ptr<ThreadPlanStack>>;
  PlansStore m_plans_sp_container;
  std::vector<ThreadPlanStack *> m_detached_plans;
  
  using PlansList = std::unordered_map<lldb::tid_t, ThreadPlanStack *>;
  PlansList m_plans_list;
};

} // namespace lldb_private

#endif // LLDB_TARGET_THREADPLANSTACK_H
