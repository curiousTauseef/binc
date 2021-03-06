#include <cmath>
#include "parser_impl.h"
#include "lookup.h"
#include "lexer.h"


namespace Bincalc {

using namespace std;


rep_type expr(bool need_get) {    // add and subtract
    ts.start_expr();
    rep_type left = term(need_get);

    while (true) {
        switch (ts.current().kind) {
            case Kind::plus: left += term(true); break;
            case Kind::minus: left -= term(true); break;
            default: return left;
        }
    }
}

rep_type term (bool need_get) {   // multiply and divide
    rep_type left = bit_term(need_get);

    while (true) {
        switch (ts.current().kind) {
            case Kind::mul: left *= bit_term(true); break;
            case Kind::div: 
                if (auto d = bit_term(true)) {  // not dividing by 0
                    left /= d;
                    break;
                }
                return error("divide by 0");
            case Kind::mod:
                if (int base = static_cast<int>(bit_term(true))) {
                    left = static_cast<int>(left) % base;
                    break;
                }
                return error("modulo by 0");
            default: return left;
        }
    }
}

rep_type bit_term (bool need_get) {   // bitwise operations
    rep_type left = unary_term(need_get);
    int left_int = left;  // ensure integral type used here; 

    while (true) {
        switch (ts.current().kind) {
		    // unsigned to use logical shift
            case Kind::lshift: left_int = (unsigned)left_int << static_cast<int>(unary_term(true)); left = left_int; break;
            case Kind::rshift: left_int = (unsigned)left_int >> static_cast<int>(unary_term(true)); left = left_int; break;
            case Kind::band: left_int &= static_cast<int>(unary_term(true)); left = left_int; break;
            case Kind::bor: left_int |= static_cast<int>(unary_term(true)); left = left_int; break;
            case Kind::bxor: left_int ^= static_cast<int>(unary_term(true)); left = left_int; break;
            default: return left;
        }
    }
}

rep_type unary_term (bool need_get) {   // unary modification of term
    rep_type left = prim(need_get);
    int left_int = left;

    while (true) {
        switch (ts.current().kind) {
            case Kind::bneg: left_int = static_cast<int>(prim(true)); left_int = ~left_int; left = left_int; break;
            case Kind::lit: left = static_cast<rep_type>(prim(true)); break;
            case Kind::pow: left = pow(left, prim(true)); break;
            // didn't call prim, so get next and return directly
            case Kind::fact: for (int i = left_int - 1; i > 1; --i) left *= i; ts.get();  return left;
            case Kind::mag_neg: left = -1 * prim(true); break;
            default: return left;
        }
    }
}

rep_type prim(bool need_get) {
    if (need_get) ts.get(); // read next token
    DEBUG("prim token: " << '[' << static_cast<char>(ts.current().kind) << ']')

    switch (ts.current().kind) {
        case Kind::fact:    // expects number to be stored 
        case Kind::number: {
            rep_type val = ts.current().number_val;
            DEBUG("primary number with value " << val);
            ts.get();
            return val;
        }
        case Kind::name: {
            // moved sicne it will not be queried afterwards
            string var_name {std::move(ts.current().string_val)};

            // function names will mask value names, look in functions first
            auto unary_func = unary_funcs.find(var_name);
            if (unary_func != unary_funcs.end()) {
                DEBUG(unary_func->first << " called (unary)")
                return unary_func->second(prim(true));
            }

            // functions that take no arguments and modify state
            auto modifier_func = modifier_funcs.find(var_name);
            if (modifier_func != modifier_funcs.end()) {
                DEBUG(modifier_func->first << " called (modifier)")
                modifier_func->second();
                // to avoid interfering with exit
                ts.get();
                suppress_print = true;
                return 0;
            }

            // else check numeric val in table
            // assigning to name
            if (ts.get().kind == Kind::assign) {
                rep_type& val = table[var_name];
                val = expr(true);
                return val;
            }
            // looking up name
            else {
                auto val_itr = table.find(var_name);
                if (val_itr != table.end()) {
                    rep_type& val = val_itr->second;  
                    DEBUG("lookup on " << var_name << ": " << val);
                    return val;
                }
                else {
                    suppress_print = true;
                    return error(var_name + " not found");
                }
            }  
        }
        case Kind::lp: {
            auto e = expr(true); // incoming expression
            // after reading in an ( expression ' ' <-- expecting rp
            if (ts.current().kind != Kind::rp) return error("')' expected");
            ts.get();    // eat ')'
            return e;
        }
        case Kind::lit: case Kind::bneg: case Kind::mag_neg: case Kind::pow:
            return rep_type{};     // unary operators
        case Kind::last: 
            if (ts.current().number_val <= history.size()) {
                rep_type past_val {history[history.size() - ts.current().number_val]};
                ts.get();   // load up next token
                return past_val;
            }
            else return error("looking past history's content");
        default: return error("primary expected");
    }
}

}   // end namespace Parser
