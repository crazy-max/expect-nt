//
// 	File:        ptrarray.h
// 	Date:        16-Jul-95
//	Description: Array of pointers
//
//   Copyright 1995 David A. Holland
//   All rights reserved except as stated in the documentation
//
//   Change log:
//
// $Log: ptrarray.h,v $
// Revision 1.1.1.1  1997/08/13 05:39:36  chaffee
// Initial release to Grant Reaber
//
// Revision 1.1  1996/07/31  05:54:45  dholland
// Initial revision
//
// Revision 1.1  1995/07/30  19:43:39  dholland
// Initial revision
//
//

#ifndef PTRARRAY_H
#define PTRARRAY_H

#ifndef assert
#include <assert.h>
#endif

#ifndef NULL
#define NULL 0
#endif

template <class T>
class ptrarray {
  protected:
    T **v;
    int n, max;
    void reallocto(int x) {
	while (max<x) max += 16;
	T **q = new T* [max];
	for (int i=0; i<n; i++) q[i] = v[i];
	delete []v;
	v = q;
    }
  public:
    ptrarray() { v=NULL; n=max=0; }
    ~ptrarray() { delete []v; }

    int num() const { return n; }

    void setsize(int newsize) {
      if (newsize>max) reallocto(newsize);
      if (newsize>n) {
	for (int i=n; i<newsize; i++) v[i] = NULL;
      }
      else {
	// do nothing
      }
      n = newsize;
    }

    T *&operator [] (int ix) const {
      assert(ix>=0 && ix<n);
      return v[ix];
    }

    int add(T *val) {
      int ix = n;
      setsize(n+1);
      v[ix] = val;
      return ix;
    }

    void push(T *val) { add(val); }

    void pop() { setsize(n-1); }
};

#endif
