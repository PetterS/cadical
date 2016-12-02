#include "clause.hpp"
#include "internal.hpp"
#include "iterator.hpp"
#include "macros.hpp"
#include "message.hpp"

#include <algorithm>

namespace CaDiCaL {

bool Internal::reducing () {
  if (!opts.reduce) return false;
  return stats.conflicts >= lim.reduce;
}

// Reason clauses (on non-zero decision level) can not be collected.
// We protect them before and unprotect them after garbage collection.

void Internal::protect_reasons () {
  for (const_int_iterator i = trail.begin (); i != trail.end (); i++) {
    Var & v = var (*i);
    if (!v.level || !v.reason) continue;
    v.reason->reason = true;
  }
}

void Internal::unprotect_reasons () {
  for (const_int_iterator i = trail.begin (); i != trail.end (); i++) {
    Var & v = var (*i);
    if (!v.level || !v.reason) continue;
    assert (v.reason->reason), v.reason->reason = false;
  }
}

// Clause with smaller glucose level (glue) are considered more useful.
// Then we use the 'analyzed' time stamp as a tie breaker.  So more recently
// resolved clauses are preferred to keep (if they have the same glue).

struct less_usefull {
  bool operator () (Clause * c, Clause * d) {
    if (c->glue > d->glue) return true;
    if (c->glue < d->glue) return false;
    return analyzed_earlier () (c, d);
  }
};

// This function implements the important reduction policy. It determines
// which redundant clauses are considered not useful and thus will be
// collected in a subsequent garbage collection phase.

void Internal::mark_useless_redundant_clauses_as_garbage () {
  vector<Clause*> stack;
  stack.reserve (stats.redundant);
  const_clause_iterator end = clauses.end (), i;
  for (i = clauses.begin (); i != end; i++) {
    Clause * c = *i;
    if (!c->redundant) continue;                 // keep irredundant
    if (c->blocked) continue;                    // keep blocked clauses
    if (c->reason) continue;                     // need to keep reasons
    if (c->garbage) continue;                    // already marked
    if (!c->have.analyzed) continue;
    if (c->analyzed () > lim.analyzed) continue;
    stack.push_back (c);
  }
  if (opts.reduceglue) sort (stack.begin (), stack.end (), less_usefull ());
  else sort (stack.begin (), stack.end (), analyzed_earlier ());

  const_clause_iterator target = stack.begin () + stack.size ()/2;
  for (const_clause_iterator i = stack.begin (); i != target; i++) {
    LOG (*i, "marking useless to be collected");
    mark_garbage (*i);
    stats.reduced++;
  }
  lim.keptsize = lim.keptglue = 0;
  end = stack.end ();
  for (i = target; i != end; i++) {
    Clause * c = *i;
    if (c->size > lim.keptsize) lim.keptsize = c->size;
    if (c->glue > lim.keptglue) lim.keptglue = c->glue;
  }
  VRB ("reduce", stats.reductions,
    "maximum kept size %d glue %d", lim.keptsize, lim.keptglue);
}

/*------------------------------------------------------------------------*/

void Internal::reduce () {
  START (reduce);
  stats.reductions++;
  report ('R', 1);
  protect_reasons ();
  mark_satisfied_clauses_as_garbage ();
  mark_useless_redundant_clauses_as_garbage ();
  garbage_collection ();
  unprotect_reasons ();
  inc.reduce += inc.redinc;
  if (inc.redinc > 1) inc.redinc--;
  lim.reduce = stats.conflicts + inc.reduce;
  lim.analyzed = stats.analyzed;
  lim.conflicts_at_last_reduce = stats.conflicts;
  report ('-');
  STOP (reduce);
}

};
