/*********************                                                        */
/*! \file theory_inference_manager.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Reynolds
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2020 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief An inference manager for Theory
 **/

#include "cvc4_private.h"

#ifndef CVC4__THEORY__THEORY_INFERENCE_MANAGER_H
#define CVC4__THEORY__THEORY_INFERENCE_MANAGER_H

#include <memory>

#include "context/cdhashset.h"
#include "expr/node.h"
#include "theory/output_channel.h"
#include "theory/theory_state.h"
#include "theory/trust_node.h"
#include "theory/uf/proof_equality_engine.h"

namespace CVC4 {

class ProofNodeManager;

namespace theory {

class Theory;
namespace eq {
class EqualityEngine;
}

/**
 * The base class for inference manager. An inference manager is a wrapper
 * around the output channel and the official equality engine of a Theory.
 * It is used for ensuring that the equality engine and output channel
 * are used properly. This includes the following invariants:
 *
 * (1) The state tracks conflicts.
 * In particular, TheoryState::isInConflict returns true whenever we have
 * called OutputChannel::conflict in the current context, which we enforce
 * by always setting d_state.notifyInConflict at the same time we send
 * conflicts on the output channel.
 *
 * (2) The theory is notified of (internal) facts.
 * In particular, Theory::preNotifyFact and Theory::notifyFact are called
 * (with isInternal = true) whenever we assert internal facts using
 * assertFactInernal below, mirroring what is done for facts from the fact
 * queue (where isInternal = false).
 *
 * (3) The proof equality engine is used whenever proofs are enabled (when
 * the proof node manager provided to this class is non-null). Notice this
 * class automatically will construct a proof equality engine during
 * setEqualityEngine, and use it for handling variants of assertInternalFact
 * below that involve proofs.
 */
class TheoryInferenceManager
{
  typedef context::CDHashSet<Node, NodeHashFunction> NodeSet;

 public:
  /**
   * Constructor, note that state should be the official state of theory t.
   */
  TheoryInferenceManager(Theory& t, TheoryState& state, ProofNodeManager* pnm);
  virtual ~TheoryInferenceManager() {}
  //--------------------------------------- initialization
  /**
   * Set equality engine, ee is a pointer to the official equality engine
   * of theory.
   */
  void setEqualityEngine(eq::EqualityEngine* ee);
  //--------------------------------------- end initialization
  /**
   * T-propagate literal lit, possibly encountered by equality engine,
   * returns false if we are in conflict.
   *
   * Note that this is the preferred method to call on
   * EqualityEngineNotify::eqNotifyTriggerPredicate and
   * EqualityEngineNotify::eqNotifyTriggerTermEquality.
   */
  bool propagateLit(TNode lit);
  /**
   * Return an explanation for the literal represented by parameter lit
   * (which was previously propagated by this theory). By default, this
   * returns the explanation given by the official equality engine of the
   * Theory, if it exists.
   */
  virtual TrustNode explainLit(TNode lit);
  /**
   * Raise conflict, called when constants a and b merge. Sends the conflict
   * on the output channel corresponding to the equality engine's explanation
   * of (= a b). The proof equality engine (if it exists) will be used as the
   * proof generator.
   *
   * Note that this is the preferred method to call on
   * EqualityEngineNotify::eqNotifyConstantTermMerge.
   */
  void conflictEqConstantMerge(TNode a, TNode b);
  /**
   * Raise conflict conf (of any form), without proofs. This method should
   * only be called if there is not yet proof support in the given theory.
   */
  void conflict(TNode conf);
  /**
   * Raise trusted conflict tconf (of any form) where a proof generator has
   * been provided in a custom way.
   */
  void trustedConflict(TrustNode tconf);
  /** Send lemma lem with property p on the output channel. */
  LemmaStatus lemma(TNode lem, LemmaProperty p = LemmaProperty::NONE);
  /** Send (trusted) lemma lem with property p on the output channel. */
  LemmaStatus trustedLemma(const TrustNode& tlem,
                           LemmaProperty p = LemmaProperty::NONE);
  /**
   * Assert internal fact. This is recommended method for asserting "internal"
   * facts into the equality engine of the theory. In particular, this method
   * should be used for anything the theory infers that is not sent on the
   * output channel as a propagation or lemma. This method ensures that the
   * Theory's preNotifyFact and notifyFact method have been called with
   * isInternal = true.
   *
   * @param atom The atom of the fact to assert
   * @param pol Its polarity
   * @param exp Its explanation, i.e. ( exp => (~) atom ) is valid.
   */
  void assertInternalFact(TNode atom, bool pol, TNode exp);
  /**
   * Assert internal fact, with a proof step justification. Notice that if
   * proofs are not enabled in this inference manager, then this asserts
   * a fact to the equality engine in the normal way.
   *
   * @param atom The atom of the fact to assert
   * @param pol Its polarity
   * @param id The proof rule identifier of the proof step
   * @param exp Its explanation, interpreted as a conjunction
   * @param args The arguments of the proof step
   */
  void assertInternalFact(TNode atom,
                          bool pol,
                          PfRule id,
                          const std::vector<Node>& exp,
                          const std::vector<Node>& args);
  /**
   * Assert internal fact, with a proof generator justification. Same as above,
   * but with a proof generator instead of an explicit step.
   *
   * @param atom The atom of the fact to assert
   * @param pol Its polarity
   * @param exp Its explanation, interpreted as a conjunction
   * @param pg The proof generator for this step. It must be the case that pf
   * can provide a proof concluding (~) atom from free asumptions in exp in
   * the remainder of the current SAT context.
   */
  void assertInternalFact(TNode atom,
                          bool pol,
                          const std::vector<Node>& exp,
                          ProofGenerator* pg);

 protected:
  /**
   * Process internal fact. This is a common helper method for the
   * assertInternalFact variants above.
   */
  void processInternalFact(TNode atom,
                           bool pol,
                           PfRule id,
                           const std::vector<Node>& exp,
                           const std::vector<Node>& args,
                           ProofGenerator* pg);
  /**
   * Explain conflict from constants merging in the equality engine. This
   * method is called by conflictEqConstantMerge. By default, it returns
   * the explanation of the official equality engine of the theory, if it
   * exists.
   */
  virtual TrustNode explainConflictEqConstantMerge(TNode a, TNode b);
  /**
   * Explain formula n (which is possibly a conjunction with no nested
   * conjunctions), add its explanation to assumptions.
   */
  void explain(TNode n, std::vector<TNode>& assumptions);
  /**
   * Explain formula n (which is possibly a conjunction with no nested
   * conjunctions), return the explanation as a conjunction.
   */
  Node mkExplain(TNode n);
  /** The theory object */
  Theory& d_theory;
  /** Reference to the state of theory */
  TheoryState& d_theoryState;
  /** Reference to the output channel of the theory */
  OutputChannel& d_out;
  /** Pointer to equality engine of the theory. */
  eq::EqualityEngine* d_ee;
  /** A proof equality engine */
  std::unique_ptr<eq::ProofEqEngine> d_pfee;
  /** The proof node manager of the theory */
  ProofNodeManager* d_pnm;
  /**
   * The keep set of this class. This set is maintained to ensure that
   * facts and their explanations are ref-counted. Since facts and their
   * explanations are SAT-context-dependent, this set is also
   * SAT-context-dependent.
   */
  NodeSet d_keep;
};

}  // namespace theory
}  // namespace CVC4

#endif /* CVC4__THEORY__THEORY_INFERENCE_MANAGER_H */
