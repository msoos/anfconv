/*****************************************************************************
anfconv
Copyright (C) 2016  Security Research Labs

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
***********************************************/

#include "simplifybysat.h"
#include "satsolve.h"
#include "cryptominisat5/cryptominisat.h"

using std::cout;
using std::endl;

SimplifyBySat::SimplifyBySat(ANF& _anf,
                             const ANF& _orig_anf,
                             ConfigData& _config,
                             uint64_t max_confl) :
        anf(_anf), orig_anf(_orig_anf), config(_config), cnf(anf, _config) {
    cnf.init();
    cnf.addAllEquations();

    //Create SAT solver
    solver = new CMSat::SATSolver();
    solver->set_verbosity(config.verbosity == 1 ? 0 : config.verbosity);
    solver->set_max_confl(max_confl);
}

SimplifyBySat::~SimplifyBySat() {
    delete solver;
}

void SimplifyBySat::addClausesToSolver() {
    for(const auto it : cnf.getClauses()) {
        for(const Clause& c : it.first) {
            vector<Lit> lits = c.getClause();
            solver->add_clause(lits);
        }
    }
}

const vector<Lit>* SimplifyBySat::extract_assumptions() const {
    vector<Lit>* assumptions = new vector<Lit>();
    for(const auto it : cnf.getAssumptions()) {
        Lit assumption_lit = it.first;
        assumptions->push_back(assumption_lit);
    }
    return assumptions;
}

int SimplifyBySat::extractUnitaries() {
    vector<Lit> units = solver->get_zero_assigned_lits();
    if (config.verbosity >= 2) {
        cout << "c Number of unit learnt clauses: " << units.size() << endl;
    }

    uint64_t numRealVarLearnt = 0;
    for(const Lit& unit : units) {
        //If var represents a partial XOR clause, skip it
        if (!cnf.varRepresentsMonomial(unit.var())) {
            continue;
        }

        BooleMonomial m = cnf.getMonomForVar(unit.var());
        assert(m.deg() > 0);

        // Monomial is high degree, and FALSE. That doesn't help much
        if (m.deg() > 1 && unit.sign() == true) {
            continue;
        }

        // If DEG is 1, then this will set the variable
        // If DEG is >0 and setting is TRUE, the addBoolePolynomial() will
        // automatically set all variables in the monomial to ONE
        BoolePolynomial poly(!unit.sign(), anf.getRing());
        poly += m;

        numRealVarLearnt += anf.addLearntBoolePolynomial(poly);
    }

    if (config.verbosity >= 1) {
        cout << "c Num ANF assignments learnt: " << numRealVarLearnt << endl;
    }
    return numRealVarLearnt;
}

int SimplifyBySat::extractBinaries() {
    vector<pair<Lit, Lit> > binXors = solver->get_all_binary_xors();
    if (config.verbosity >= 2) {
        cout << "c Number of binary clauses:" << binXors.size() << endl;
    }

    uint64_t numRealVarReplaced = 0;
    for(pair<Lit, Lit>& pair : binXors) {
        Lit lit1 = pair.first;
        Lit lit2 = pair.second;
        uint32_t v1 = lit1.var();
        uint32_t v2 = lit2.var();
        assert(v1 != v2);

        //If any of the two vars represent a partial XOR clause, skip it
        if (!cnf.varRepresentsMonomial(v1) || !cnf.varRepresentsMonomial(v2)) {
            continue;
        }

        BooleMonomial m1 = cnf.getMonomForVar(v1);
        BooleMonomial m2 = cnf.getMonomForVar(v2);

        // Not anti/equivalence
        if (m1.deg() > 1 || m2.deg() > 1) {
            continue;
        }

        BoolePolynomial poly((lit1.sign() ^ lit2.sign()), anf.getRing());
        poly += m1;
        poly += m2;

        numRealVarReplaced += anf.addLearntBoolePolynomial(poly);
    }

    if (config.verbosity >= 1) {
        cout << "c Num ANF anti/equivalence learnt: " << numRealVarReplaced << endl;
    }
    return numRealVarReplaced;
}

bool SimplifyBySat::addPolynomial(const pair<vector<uint32_t>, bool>& cnf_poly) {
    BoolePolynomial new_poly(cnf_poly.second, anf.getRing());
    for (const uint32_t& var_idx : cnf_poly.first) {
        if (!cnf.varRepresentsMonomial(var_idx)) {
            return false;
        }
        new_poly += cnf.getMonomForVar(var_idx);
    }

    if (new_poly.deg() == 1) {
        return anf.addLearntBoolePolynomial(new_poly);
    }
    return false;
}

int SimplifyBySat::process(const vector< pair<vector<uint32_t>, bool> >& extracted) {
    int num_new_polys = 0;
    for (const pair<vector<uint32_t>, bool>& cnf_poly : extracted) {
        bool added = addPolynomial(cnf_poly);
        if (added) {
            num_new_polys++;
        }
    }
    return num_new_polys;
}

int SimplifyBySat::extractLinear() {
    int num_new_polys = 0;
    num_new_polys += process(solver->get_recovered_xors(false));
    num_new_polys += process(solver->get_recovered_xors(true));

    if (config.verbosity >= 1) {
        cout << "c Num ANF linear equations learnt: " << num_new_polys << endl;
    }
    return num_new_polys;
}

int SimplifyBySat::simplify()
{
    if (!anf.getOK()) {
        cout << "Nothing to simplify: UNSAT" << endl;
        return -1;
    }

    //Add variables to SAT solver
    for(uint32_t i = 0; i < cnf.getNumVars(); i++) {
        solver->new_var();
    }

    //Add XOR & normal clauses from CNF
    addClausesToSolver();
    const vector<Lit>* assumptions = extract_assumptions();
    // for (auto x : *assumptions) {
    //     cout << x << endl;
    // }

    //Solve system of CNF until conflict limit
    cout << "c Converted CNF has "
         << cnf.getNumVars() << " variables and "
         << cnf.getNumClauses() << " clauses" << endl;
    lbool ret = solver->solve(assumptions);

    //Extract data
    int num_learnt = 0;
    num_learnt += extractUnitaries();
    num_learnt += extractBinaries();
    num_learnt += extractLinear();

    if (ret == l_Undef) {
        return num_learnt;
    }

    if (ret == l_False) {
        cout << "c UNSAT returned by solver" << endl;
        anf.addBoolePolynomial(BoolePolynomial(true, anf.getRing()));

        return -1;
    }

    //We have a solution
    assert(ret == l_True);
    map<uint32_t, lbool> solutionFromSolver;
    for(uint32_t i = 0; i < solver->get_model().size(); i++) {
        solutionFromSolver[i] = solver->get_model()[i];
        assert(solver->get_model()[i] != l_Undef);
        if (config.verbosity >= 3) {
            cout << ((solver->get_model()[i] == l_True) ? "" : "-") << i << " ";
        }
    }
    if (config.verbosity >= 3) {
        cout << endl;
    }

    vector<lbool> solution2 = cnf.mapSolToOrig(solutionFromSolver);
    const vector<lbool> solution = anf.extendSolution(solution2);

    if (config.verbosity >= 2) {
        printSolution(solution);
    }
    testSolution(orig_anf, solution);

    return num_learnt;
}
