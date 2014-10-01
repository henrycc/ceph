// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2014 Red Hat
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "include/Context.h"
#include <set>

class Continuation {
  std::set<int> stages_in_flight;
  std::set<int> activated_stages;
  int rval;
  Context *on_finish;

  class Callback : public Context {
    Continuation *continuation;
    int stage_to_activate;
  public:
    Callback(Continuation *c, int stage) :
      continuation(c),
      stage_to_activate(stage) {}
    void finish(int r) {
      continuation->continue_function(r, stage_to_activate);
    }
  };

protected:
  typedef bool (Continuation::*stagePtr)(int r, int n);
  bool immediate(int stage, int r) {
    assert(!stages_in_flight.count(stage));
    stages_in_flight.insert(stage);
    return _continue_function(r, stage);
  }

  Context *get_callback(int stage) {
    stages_in_flight.insert(stage);
    return new Callback(this, stage);
  }

  void set_rval(int new_rval) { rval = new_rval; }

  void set_callback(int stage, stagePtr func) {
    assert(callbacks.find(stage) == callbacks.end());
    callbacks[stage] = func;
  }

private:
  std::map<int, Continuation::stagePtr> callbacks;

  bool _continue_function(int r, int n) {
    set<int>::iterator stage_iter = stages_in_flight.find(n);
    assert(stage_iter != stages_in_flight.end());
    assert(callbacks.count(n));
    stagePtr p = callbacks[n];
    bool done = (this->*p)(r, n);
    stages_in_flight.erase(stage_iter);
    return done;
  }

  void continue_function(int r, int stage) {
    bool done = _continue_function(r, stage);

    assert (!done || stages_in_flight.empty());
    if (done) {
      on_finish->complete(rval);
      on_finish = NULL;
      delete this;
      return;
    }
  }



public:
  Continuation(Context *c) :
    rval(0), on_finish(c) {}
  virtual ~Continuation() { assert(on_finish == NULL); }

  void begin() { stages_in_flight.insert(0); continue_function(0, 0); }
};
