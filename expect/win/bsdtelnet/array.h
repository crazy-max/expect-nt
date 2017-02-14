//
// 	File:        array.h
// 	Date:        16-Jul-95
//	Description: array template
//
//   Copyright 1995 David A. Holland
//   All rights reserved except as stated in the documentation
//
//   Change log:
//
// $Log: array.h,v $
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

#ifndef ARRAY_H
#define ARRAY_H

#ifndef assert
#include <assert.h>
#endif

#ifndef NULL
#define NULL 0
#endif

inline void *operator new(unsigned int, void *v) { return v; }

template <class T>
class array {
  protected:
    T *v;
    int n, max;

    void reallocto(int newsize) {
      while (max<newsize) max += 16;
      char *x = new char[max*sizeof(T)];
      memcpy(x,v,n*sizeof(T));
      delete []((char *)v);
      v = (T *) x;
    }
  public:
    array() { v=NULL; n=max=0; }
    ~array() { delete v; }

    int num() const { return n; }

    void setsize(int newsize) {
      if (newsize>max) reallocto(newsize);
      if (newsize>n) {
	// call default constructors
	for (int i=n; i<newsize; i++) (void) new(&v[i]) T;
      }
      else {
	// call destructors
	for (int i=newsize; i<n; i++) v[i].~T();
      }
      n = newsize;
    }

    T &operator [] (int ix) const {
      assert(ix>=0 && ix<n);
      return v[ix];
    }

    int add(const T &val) {
      int ix = n;
      setsize(n+1);
      v[ix] = val;
      return ix;
    }

    void push(const T &val) { add(val); }

    T pop() { T t = (*this)[n-1]; setsize(n-1); return t; }
};

#endif
