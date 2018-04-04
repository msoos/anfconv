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

#include "karnaugh.h"
#include "evaluator.h"
#include <iostream>

using std::cout;
using std::endl;
using std::map;

//#define DEBUG_KARNAUGH

vector<Clause> Karnaugh::getClauses()
{
    vector<Clause> clauses;
    for (int i = 0; i < no_lines[1]; i++) {
        assert(output[i][0] == 1);
        vector<Lit> lits;

        /*
        for (size_t i2 = 0; i2 < tableVars.size(); i2++) {
            cout << input[i][i2] << " ";
        }
        cout << endl;*/

        for (size_t i2 = 0; i2 < tableVars.size(); i2++) {
            if (input[i][i2] != 2)
                lits.push_back(Lit(tableVars[i2], input[i][i2]));
        }

        clauses.push_back(Clause(lits));
    }

    return clauses;
}

void Karnaugh::print() const
{
    for (int i = 0; i < no_lines[0] + no_lines[1]; i++) {
        for (size_t i2 = 0; i2 < tableVars.size(); i2++) {
            cout << input[i][i2] << " ";
        }
        cout << "-- " << output[i][0]  << endl;
    }
    cout << "--------------" << endl;
}

bool Karnaugh::possibleToConv(const BoolePolynomial& eq)
{
    tableVars.clear();
    BooleMonomial m = eq.usedVariables();
    for(BooleMonomial::const_iterator
        it = m.begin(), end = m.end()
        ; it != end
        ; it++
    ) {
        tableVars.push_back(*it);
    }
    return (maxKarnTable >= tableVars.size());
}

void Karnaugh::evaluateIntoKarn(const BoolePolynomial& eq)
{
    //Move vars to tablevars
    tableVars.clear();
    BooleMonomial m = eq.usedVariables();
    for(BooleMonomial::const_iterator
        it = m.begin(), end = m.end()
        ; it != end
        ; it++
    ) {
        tableVars.push_back(*it);
    }

    //check if we can at all represent it
    if (maxKarnTable < tableVars.size()) {
        cout << "equation: " << eq << " (vars in poly:";
        std::copy(tableVars.begin(), tableVars.end(),
             std::ostream_iterator<uint32_t>(cout, ","));
        cout << ")" << endl;
        cout << "max_var in equationstosat.cpp is too small!" << endl;
        exit(-1);
    }

    map<uint32_t, uint32_t> outerToInter;
    uint32_t i = 0;
    for (vector<uint32_t>::const_iterator
        it = tableVars.begin(), end = tableVars.end()
        ; it != end
        ; it++, i++
    ) {
        outerToInter[*it] = i;
    }

    //Put in the whole truth table
    no_lines[0] = 0;
    no_lines[1] = 0;
    no_lines[2] = 0;
    for (uint32_t setting = 0
        ; setting < ((uint32_t)0x1 << tableVars.size())
        ; setting++
    ) {
        //Variable setting
        for (size_t i = 0; i < tableVars.size(); i++) {
            uint val = (setting >> i) & 0x1;
            input[no_lines[1]][i] = val;

            /*#ifdef DEBUG_KARNAUGH
            cout << val;
            #endif*/
        }
        //cout << " --- ";

        //Evaluate now
        lbool lout = evaluatePoly(eq, setting, outerToInter);
        assert(lout != l_Undef);
        bool out = (lout == l_True);
        if (out) {
            output[no_lines[1]][0] = 1;
            no_lines[1]++;
        }
    }
    #ifdef DEBUG_KARNAUGH
    cout << "KARNAUGH after eval:" << endl;
    print();
    #endif
}

vector<Clause> Karnaugh::convert(const BoolePolynomial& eq)
{
    evaluateIntoKarn(eq);
    minimise_karnaugh(tableVars.size(), 1, input, output, no_lines, false);
    #ifdef DEBUG_KARNAUGH
    cout << "KARNAUGH after minim:" << endl;
    print();
    #endif
    return getClauses();
}

void Karnaugh::debugConv(const BoolePolynomial& eq)
{
    if (possibleToConv(eq)) {
        vector<Clause> clauses = convert(eq);
        cout << "Converting: \"" << eq << "\" to karn. Clauses: " << endl;
        for(vector<Clause>::const_iterator it = clauses.begin(), end = clauses.end(); it != end; it++) {
            cout << *it << endl;
        }
    } else {
        cout << "Cannot convert eq" << eq << endl;
    }
}

void Karnaugh::test()
{
    BoolePolyRing myring(100);
    BoolePolynomial eq(myring);
    eq += BooleVariable(5, myring);
    eq += BooleVariable(6, myring) * BooleVariable(5, myring);
    debugConv(eq);

    BoolePolynomial eq2(myring);
    eq2 += BooleVariable(5, myring)*BooleVariable(1, myring);
    eq2 += BooleVariable(6, myring)*BooleVariable(5, myring);
    debugConv(eq2);

    debugConv(eq);
}
