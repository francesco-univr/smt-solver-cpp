#include "parser.h"
#include "theory_solver.h"
#include <iostream>
#include <fstream>
#include <sstream>

void solve_and_print(const std::string& input) {
    try {
        std::cout << "Parsing..." << std::endl;
        Parser parser(input);
        std::vector<std::vector<Literal>> contexts = parser.parse();
        std::vector<TermPtr> terms = parser.getTerms();
        std::cout << "Parsing done. Contexts: " << contexts.size() << std::endl;

        std::cout << "Solving..." << std::endl;
        bool any_sat = false;
        for (const auto& literals : contexts) {
            TheorySolver solver(literals, terms);
            if (solver.solve()) {
                any_sat = true;
                break;
            }
        }
        std::cout << "Solving done." << std::endl;

        if (any_sat) {
            std::cout << "SAT" << std::endl;
        } else {
            std::cout << "UNSAT" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Error: Unknown exception caught." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::ifstream infile(argv[1]);
        if (!infile) {
            std::cerr << "Error: Could not open file " << argv[1] << std::endl;
            return 1;
        }
        std::stringstream buffer;
        buffer << infile.rdbuf();
        solve_and_print(buffer.str());
    } else {
        std::cout << "SMT Solver Interactive Mode" << std::endl;
        std::cout << "Enter S-expressions." << std::endl;
        std::cout << "Type 'END' on a new line to solve the current block." << std::endl;
        std::cout << "Type 'EXIT' on a new line to quit." << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        
        while (true) {
            std::string input;
            std::string line;
            bool stop = false;
            
            std::cout << "> "; 
            while (std::getline(std::cin, line)) {
                if (line == "END") break;
                if (line == "EXIT") { stop = true; break; }
                input += line + "\n";
            }
            
            if (stop) break;
            if (input.empty()) continue;

            solve_and_print(input);
            std::cout << "--------------------------------------------------" << std::endl;
        }
    }

    return 0;
}
