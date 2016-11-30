#ifndef _clause_hpp_INCLUDED
#define _clause_hpp_INCLUDED

#include <cstdlib>
#include <cassert>

#include "iterator.hpp"

namespace CaDiCaL {

// The 'Clause' data structure is very important. There are usually many
// clauses and accessing them is a hot-spot.  Thus we use three common
// optimizations to reduce their memory foot print and improve cache usage.
// Even though this induces some complexity in understanding the actual
// implementation, arguably not the usage of this data-structure, we deem
// these optimizations for essential.
//
//   (1) The most important optimization is to 'embed' the actual literals
//   in the clause.  This requires a variadic size structure and thus
//   strictly is not 'C' conformant, but supported by all compilers we used.
//   The alternative is to store the actual literals somewhere else, which
//   not only needs more memory but more importantly also requires another
//   memory access and thus is very costly.
//
//   (2) The boolean flags only need one bit each and thus there is enough
//   space left to merge them with a 'glue' bit field (which is less
//   accessed than 'size').  This saves 4 bytes and also keeps the header
//   without 'analyzed' nicely in 8 bytes.  We currently use 28 bits and
//   actually since we do not want to mess with 'unsigned' versus 'signed'
//   issues just use 27 out of them.  If more boolean flags are needed this
//   number has to be adapted accordingly.
//
//   (3) Original clauses and clauses with small glue or size are kept
//   anyhow and do not need the activity counter 'analyzed'.  Thus we can
//   omit these 8 bytes used for 'analyzed' for these clauses.  Redundant
//   clauses of long size and with large glue have a 'analyzed' field and
//   are called 'extended'.  The non extended clauses need 8 bytes less and
//   accessing 'analyzed' for them is not allowed.
//
// With these three optimizations a binary original clause only needs 16
// bytes instead of 40 bytes without embedding and 32 bytes with embedding
// the literals.  The last two optimizations reduce memory usage of very
// large formulas slightly but are otherwise not that important.
//
// If you want to add few additional boolean flags, add them after the
// sequence of already existing ones.  This makes sure that these bits and
// the following 'glue' field are put into a 4 byte word by the compiler. Of
// course you need to adapt 'LD_MAX_GLUE' accordingly.  Adding one flag
// reduces it by one.
//
// Similarly if you want to add more data to extended clauses put these
// fields after 'analyzed' and before the flags section.  Then adapt the
// 'EXTENDED_OFFSET' accordingly.
//
// Additional fields needed for all clauses are safely put between 'glue'
// and 'size' without the need to change anything else.  In general these
// optimizations are local to 'clause.[hc]pp' and otherwise can be ignored
// except that you should for instance never access 'analyzed' of a clauses
// which is not extended.  This can be checked with for instance 'valgrind'
// but is also guarded by making the actual '_analyzed' field private and
// checking this contract in the 'analyzed ()' accessors functions.

#define LD_MAX_GLUE 25
#define MAX_GLUE ((1 << (LD_MAX_GLUE-1)) - 1)

class Clause {

public:

  long _analyzed;   // time stamp when analyzed last time if redundant
  int _pos;         // position of last watch replacement

  struct { bool analyzed : 1; bool pos : 1; } have;

  bool redundant:1; // aka 'learned' so not 'irredundant' (original)
  bool garbage:1;   // can be garbage collected unless it is a 'reason'
  bool reason:1;    // reason / antecedent clause can not be collected
  bool moved:1;     // moved during garbage collector ('copy' valid)

  signed int glue : LD_MAX_GLUE;

  int blocked;      // blocking literal ...

  int size;         // actual size of 'literals' (at least 2)

  union {

    int literals[2];    // of variadic 'size' (not just 2) in general

    Clause * copy;      // only valid if 'moved', then that's where to

    // The 'copy' field is only used for 'moved' clauses in 'copy_clause'
    // in the moving garbage collector 'copy_non_garbage_clauses'.
    // Otherwise 'literals' is valid.
  };

  long & analyzed () {
    assert (have.analyzed);
    return _analyzed;
  }

  const long & analyzed () const {
    assert (have.analyzed);
    return _analyzed;
  }

  int & pos () { assert (have.pos); return _pos; }
  const int & pos () const { assert (have.pos); return _pos; }

  void update_after_shrinking () { 
    assert (size >= 2);
    if (have.pos && _pos >= size) _pos = 2;
    if (glue > size) glue = size;
  }

  literal_iterator       begin ()       { return literals; }
  literal_iterator         end ()       { return literals + size; }

  const_literal_iterator begin () const { return literals; }
  const_literal_iterator   end () const { return literals + size; }

  // Actual start of allocated memory, bytes allocated and offset are only
  // used for memory (de)allocation in 'delete_clause' and in the moving
  // garbage collector 'copy_non_garbage_clauses' and 'copy_clause'.
  //
  char * start () const;        // actual start of allocated memory
  size_t bytes () const;        // actual number of bytes allocated
  size_t offset () const;       // offset of valid bytes (start - this)

  // Check whether this clause is ready to be collected and deleted.  The
  // 'reason' flag is only there for protecting reason clauses in 'reduce',
  // which does not backtrack to the root level.  If garbage collection is
  // triggered from a preprocessor, which backtracks to the root level, then
  // 'reason' is false for sure. We want to use the same garbage collection
  // code though for both situations and thus hide here this variance.
  //
  bool collect () const { return !reason && garbage; }
};

struct analyzed_earlier {
  bool operator () (const Clause * a, const Clause * b) {
    return a->analyzed () < b->analyzed ();
  }
};

struct smaller_size {
  bool operator () (const Clause * a, const Clause * b) {
    return a->size < b->size;
  }
};

/*------------------------------------------------------------------------*/

inline size_t Clause::offset () const {
  size_t res = 0;
  if (!have.pos) res += sizeof _pos;
  if (!have.analyzed) res += sizeof _analyzed;
  return res;
}

inline size_t Clause::bytes () const {
  return sizeof (Clause) + (size - 2) * sizeof (int) - offset ();
}

inline char * Clause::start () const {
  return offset () + (char*) this;
}

/*------------------------------------------------------------------------*/

// Place literals over the same variable close to each other.  This allows
// eager removal of identical literals and detection of tautological clauses
// and also is easier to read for debugging.

struct lit_less_than {
  bool operator () (int a, int b) const {
    int s = abs (a), t = abs (b);
    return s < t || (s == t && a < b);
  }
};

};

#endif
