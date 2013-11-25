/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <string>
#include "util/test.h"
#include "util/interrupt.h"
#include "kernel/builtin.h"
#include "library/all/all.h"
#include "library/tactic/goal.h"
#include "library/tactic/proof_builder.h"
#include "library/tactic/proof_state.h"
#include "library/tactic/tactic.h"
#include "library/tactic/boolean.h"
using namespace lean;

tactic loop_tactic() {
    return mk_tactic1([=](environment const &, io_state const &, proof_state const & s) -> proof_state {
            while (true) {
                check_interrupted();
            }
            return s;
        });
}

tactic set_tactic(std::atomic<bool> * flag) {
    return mk_tactic1([=](environment const &, io_state const &, proof_state const & s) -> proof_state {
            *flag = true;
            return s;
        });
}

tactic show_opts_tactic() {
    return mk_tactic1([=](environment const &, io_state const & io, proof_state const & s) -> proof_state {
            io.get_diagnostic_channel() << "options: " << io.get_options() << "\n";
            io.get_diagnostic_channel().get_stream().flush();
            return s;
        });
}

static void check_failure(tactic t, environment const & env, io_state const & io, context const & ctx, expr const & ty) {
    try {
        t.solve(env, io, ctx, ty);
        lean_unreachable();
    } catch (exception & ex) {
        std::cout << "expected error: " << ex.what() << "\n";
    }
}

static void tst1() {
    environment env;
    io_state io(options(), mk_simple_formatter());
    import_all(env);
    env.add_var("p", Bool);
    env.add_var("q", Bool);
    expr p = Const("p");
    expr q = Const("q");
    context ctx;
    ctx = extend(ctx, "H1", p);
    ctx = extend(ctx, "H2", q);
    proof_state s = to_proof_state(env, ctx, p);
    std::cout << s.pp(mk_simple_formatter(), options()) << "\n";
    tactic t = then(assumption_tactic(), now_tactic());
    std::cout << "proof 1: " << t.solve(env, io, s) << "\n";
    std::cout << "proof 2: " << t.solve(env, io, ctx, q) << "\n";
    check_failure(now_tactic(), env, io, ctx, q);
    std::cout << "proof 2: " << orelse(fail_tactic(), t).solve(env, io, ctx, q) << "\n";

#ifndef __APPLE__
    check_failure(try_for(loop_tactic(), 100), env, io, ctx, q);
    std::cout << "proof 1: " << try_for(t, 10000).solve(env, io, s) << "\n";
    check_failure(try_for(orelse(try_for(loop_tactic(), 10000),
                                 trace_tactic(std::string("hello world"))),
                          100),
                  env, io, ctx, q);
    std::atomic<bool> flag1(false);
    check_failure(try_for(orelse(try_for(loop_tactic(), 10000),
                                 set_tactic(&flag1)),
                          100),
                  env, io, ctx, q);
    lean_assert(!flag1);
    std::cout << "Before nested try_for...\n";
    check_failure(orelse(try_for(try_for(loop_tactic(), 10000), 100),
                         set_tactic(&flag1)),
                  env, io, ctx, q);
    lean_assert(flag1);
    std::cout << "Before parallel 3 parallel tactics...\n";
    std::cout << "proof 2: " << par(loop_tactic(), par(loop_tactic(), t)).solve(env, io, ctx, q) << "\n";
#endif
    std::cout << "Before hello1 and 2...\n";
    std::cout << "proof 2: " << orelse(then(repeat_at_most(append(trace_tactic("hello1"), trace_tactic("hello2")), 5), fail_tactic()),
                                       t).solve(env, io, ctx, q) << "\n";
    std::cout << "------------------\n";
    std::cout << "proof 2: " << ((trace_tactic("hello1.1") + trace_tactic("hello1.2") + trace_tactic("hello1.3") + trace_tactic("hello1.4")) <<
                                 (trace_tactic("hello2.1") + trace_tactic("hello2.2")) <<
                                 (trace_tactic("hello3.1") || trace_tactic("hello3.2")) <<
                                 assumption_tactic()).solve(env, io, ctx, q) << "\n";
    std::cout << "------------------\n";
    std::cout << "proof 2: " << then(cond([](environment const &, io_state const &, proof_state const &) { return true; },
                                          trace_tactic("then branch.1") + trace_tactic("then branch.2"),
                                          trace_tactic("else branch")),
                                     t).solve(env, io, ctx, q) << "\n";

    std::cout << "proof 2: " << then(when([](environment const &, io_state const &, proof_state const &) { return true; },
                                          trace_tactic("when branch.1") + trace_tactic("when branch.2")),
                                     t).solve(env, io, ctx, q) << "\n";
    std::cout << "------------------\n";
    std::cout << "proof 2: " << (suppress_trace(trace_tactic("msg1") << trace_tactic("msg2")) <<
                                 trace_tactic("msg3") << t).solve(env, io, ctx, q) << "\n";
    std::cout << "------------------\n";
    std::cout << "proof 2: " << (show_opts_tactic() << using_params(show_opts_tactic(), options(name({"pp", "colors"}), true)) <<
                                 show_opts_tactic() << t).solve(env, io, ctx, q) << "\n";
    std::cout << "done\n";
}

static void tst2() {
    environment env;
    io_state io(options(), mk_simple_formatter());
    import_all(env);
    env.add_var("p", Bool);
    env.add_var("q", Bool);
    expr p = Const("p");
    expr q = Const("q");
    context ctx;
    ctx = extend(ctx, "H1", p);
    ctx = extend(ctx, "H2", q);
    std::cout << "proof: " << (repeat(conj_tactic()) << assumption_tactic()).solve(env, io, ctx, And(And(p, q), And(p, p)))
              << "\n";
    std::cout << "-------------\n";
    // Theorem to be proved
    expr F   = Implies(p, Implies(q, And(And(p, q), And(p, p))));
    // Tactic
    tactic T = repeat(conj_tactic() || imp_tactic()) << trace_state_tactic() << assumption_tactic();
    // Generate proof using tactic
    expr pr  = T.solve(env, io, context(), F);
    // Print proof
    std::cout << pr << "\n";
    // Check whether the proof is correct or not.
    std::cout << env.infer_type(pr) << "\n";
}

int main() {
    tst1();
    tst2();
    return has_violations() ? 1 : 0;
}
