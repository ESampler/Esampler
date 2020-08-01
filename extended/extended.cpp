#include <string.h>
#include <z3++.h>
#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <queue>
using namespace std;

class QuickSampler {
    std::string input_file;

    struct timespec start_time;
    double solver_time = 0.0;
    int max_samples;
    double max_time;
    bool enable_derive;

    z3::context c;
    z3::optimize opt;
    z3::solver slv;
    std::vector<int> ind;
    std::vector<int> var;
    std::unordered_set<int> unsat_vars;
    int epochs = 0;
    int drv_rounds = 0;
    int drv_number = 0;
    int wrong_num = 0;
    int flips = 0;
    int samples = 0;
    int solver_calls = 0;
    std::unordered_set<string> solution_set;
    bool has_large_derivation = false;
    std::ofstream results_file;
    int max_drv = 10000;
    unordered_map<int, unordered_set<int>> clause_of_literal;

public:
    QuickSampler(std::string input, int max_samples, double max_time, bool enable_derive) : opt(c), slv(c), input_file(input), max_samples(max_samples), max_time(max_time), enable_derive(enable_derive){}

    void run() {
        clock_gettime(CLOCK_REALTIME, &start_time);
        srand(start_time.tv_sec);
        parse_cnf();
        results_file.open(input_file + ".samples");
        int unSAT = 0;
        while (unSAT < 10) {
            opt.push();
            for (int v : ind) {
                if (rand() % 2)
                    opt.add(literal(v), 1);
                else
                    opt.add(!literal(v), 1);
            }
            if (!solve()) {
                std::cout << "Could not find a solution!\n";
		exit(0);
	    }
            z3::model m = opt.get_model();
            opt.pop();
            string tmp = model_string(m, ind);
            if (solution_set.find(tmp)==solution_set.end()){
                unSAT = 0;
                sample(m);
                print_stats(false);
            }
            else
                unSAT += 1;
        }
    }

    void print_stats(bool simple) {
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);
        double elapsed = duration(&start_time, &end);
        std::cout << "Samples: " << samples << '\n';
        std::cout << "Execution time: " << elapsed << '\n';
        cout << "Derived samples: "<< drv_number <<endl;
        if (simple)
            return;
        cout << "Derived rounds: "<<drv_rounds <<endl;
        cout << "Wrong number: " << wrong_num <<endl;
        std::cout << "Solver time: " << solver_time << '\n';
        std::cout << "Epochs: " << epochs << ", Flips: " << flips << ", Unsat: " << unsat_vars.size() << ", Calls: " << solver_calls << '\n';
    }

    void parse_cnf() {
        z3::expr_vector exp(c);
        std::ifstream f(input_file);
        if (!f.is_open()) {
            std::cout << "Error opening input file\n";
            abort();
        }
        std::unordered_set<int> indset, varset;
        bool has_ind = false;
        int max_var = 0;
        std::string line;
        int counter = 0;
        while (getline(f, line)) {
            std::istringstream iss(line);
            if(line.find("c ind ") == 0) {
                std::string s;
                iss >> s;
                iss >> s;
                int v;
                while (!iss.eof()) {
                    iss >> v;
                    if (v && indset.find(v) == indset.end()) {
                        indset.insert(v);
                        ind.push_back(v);
                        has_ind = true;
                    }
                }
            } else if (line[0] != 'c' && line[0] != 'p') {
                z3::expr_vector clause(c);
                int v, l;
                while (!iss.eof()) {
                    iss >> l;
                    if (l != 0){
                        clause.push_back(literal(l));
                        v = abs(l);
                        if (varset.find(v)==varset.end())
                            varset.insert(v);
                        if (clause_of_literal.find(l) == clause_of_literal.end()) {
                            unordered_set<int> clause_ids{ counter };
                            clause_of_literal.insert(std::make_pair(l, clause_ids));
                        }
                        else {
                            clause_of_literal[l].insert(counter);
                        }
                        if (!has_ind && v != 0)
                            indset.insert(v);
                        if (v > max_var)
                            max_var = v;
                    }
                }
                if (clause.size() > 0)
                    counter += 1;
		            exp.push_back(mk_or(clause));
            }
        }
        f.close();
        for (int v: varset)
            var.push_back(v);
        if (!has_ind) {
            for (int lit = 0; lit <= max_var; ++lit) {
                if (indset.find(lit) != indset.end()) {
                    ind.push_back(lit);
                }
            }
        }
        z3::expr formula = mk_and(exp);
        opt.add(formula);
        slv.add(formula);
    }

    void sample(z3::model m) {
        std::unordered_set<std::string> initial_mutations;
        std::string m_string = model_string(m, ind);
        string v_all = model_string(m, var);
        std:: cout << m_string << " STARTING\n";
        output(m_string, 0, true, false);
        if (enable_derive && (has_large_derivation || drv_rounds <= 3))
            derive(v_all, m_string);
        opt.push();
        for (int i = 0; i < ind.size(); ++i) {
            int v = ind[i];
            if (m_string[i] == '1')
                opt.add(literal(v), 1);
            else
                opt.add(!literal(v), 1);
        }

        std::unordered_map<std::string, int> mutations;
        for (int i = 0; i < ind.size(); ++i) {
            if (unsat_vars.find(i) != unsat_vars.end())
                continue;
            opt.push();
            int v = ind[i];
            if (m_string[i] == '1')
                opt.add(!literal(v));
            else
                opt.add(literal(v));
            if (solve()) {
                z3::model new_model = opt.get_model();
                std::string new_string = model_string(new_model, ind);
                if (solution_set.find(new_string) != solution_set.end()){
                    opt.pop();
                    continue;
                }
                if (initial_mutations.find(new_string) == initial_mutations.end()) {
                    initial_mutations.insert(new_string);
                    //std::cout << new_string << '\n';
                    std::unordered_map<std::string, int> new_mutations;
                    new_mutations[new_string] = 1;
                    output(new_string, 1, true, false);
                    string v_all;
                    if (enable_derive && (has_large_derivation || drv_rounds <= 3))
                        v_all = model_string(new_model, var);
                    if  (enable_derive && (has_large_derivation || drv_rounds <= 3))
                        derive(v_all, new_string);
                    flips += 1;
                    for (auto it : mutations) {
                        if (it.second >= 6)
                            continue;
                        std::string candidate;
                        for (int j = 0; j < ind.size(); ++j) {
                            bool a = m_string[j] == '1';
                            bool b = it.first[j] == '1';
                            bool c = new_string[j] == '1';
                            if (a ^ ((a^b) | (a^c)))
                                candidate += '1';
                            else
                                candidate += '0';
                        }
                        if (mutations.find(candidate) == mutations.end() && new_mutations.find(candidate) == new_mutations.end()) {
                            string v_all = output(candidate, it.second + 1, false, false);
                            if (v_all != ""){
                                new_mutations[candidate] = it.second + 1;
                                if  (enable_derive && (has_large_derivation || drv_rounds <= 3))
                                    derive(v_all, candidate);
                            }
                        }
                    }
                    for (auto it : new_mutations) {
                        mutations[it.first] = it.second;
                    }
                } else {
                    //std::cout << new_string << " repeated\n";
                }
            } else {
                std::cout << "unsat\n";
                unsat_vars.insert(i);
            }
            opt.pop();
            print_stats(true);
        }
        epochs += 1;
        opt.pop();
    }

    std::string output(std::string sample, int nmut, bool right, bool drv) {
        if (solution_set.find(sample) == solution_set.end()){
            if (!right){
                slv.push();
                for (int i=0;i<ind.size(); i++){
                    int tmp = 2 * (sample[i] - '0') - 1;
                    int l = tmp*ind[i];
                    slv.add(literal(l));
                }
                if (slv.check() == z3::sat){
                    std::string v_all;
                    if  (enable_derive && (has_large_derivation || drv_rounds <= 3))
                        v_all = model_string(slv.get_model(), var);
                    else
                        v_all = "1";
                    slv.pop();
                    solution_set.insert(sample);
                    samples += 1;
                    results_file << nmut << ": " << sample << '\n';
                    return v_all;
                }
                else{
                    slv.pop();
                    wrong_num += 1;
                    return "";
                }
            }
            else{
                solution_set.insert(sample);
                samples += 1;
                results_file << nmut << ": " << sample << '\n';
                return "";
            }
        }
        else
            return "";
    }

    void finish() {
        print_stats(false);
        results_file.close();
        exit(0);
    }

    bool solve() {
        struct timespec start;
        clock_gettime(CLOCK_REALTIME, &start);
        double elapsed = duration(&start_time, &start);
        if (elapsed > max_time) {
            std::cout << "Stopping: timeout\n";
            finish();
        }
        if (samples >= max_samples) {
            std::cout << "Stopping: samples\n";
            finish();
        }

        z3::check_result result = opt.check();
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);
        solver_time += duration(&start, &end);
        solver_calls += 1;

        return result == z3::sat;
    }

	std::string model_string(z3::model model, vector<int>& vec) {
		std::string s;
		for (int v : vec) {
			z3::func_decl decl(literal(v).decl());
			z3::expr b = model.get_const_interp(decl);
			if (b.bool_value() == Z3_L_TRUE) {
				s += "1";
			}
			else {
				s += "0";
			}
		}
		return s;
	}

	void derive(string&  v_all, string v_ind) {
		//clock_gettime(CLOCK_REALTIME, &start);
		//if (debug)
		//	cout << "enter derivation" << endl;
		queue<string> Q;
		Q.push(v_ind);
		queue<unordered_set<int>> changed_bits_queue;
		unordered_set<int> changed_bits;
		changed_bits_queue.push(changed_bits);
		unordered_map<int, int> ones_counter;
		string new_v;
		int new_derivation_number = 0;
        drv_rounds += 1;
		for (int i = 0; i < var.size(); i++) {
			int tmp = 2 * (v_all[i] - '0') - 1;
			int l = tmp*var[i];
			for (int clause : clause_of_literal[l]) {
				if (ones_counter.find(clause) == ones_counter.end())
					ones_counter.insert(make_pair(clause, 1));
				else
					ones_counter[clause] += 1;
			}
		}
		unordered_map<int, int> tmp_ones_counter;
		//unordered_set<string> tmp_models;
		bool stop = false;

		int ind_size = ind.size();
		vector<int> shuffle_order (ind_size, 0);
		for (int k = 0; k < ind_size; k++)
			shuffle_order[k] = k;
		random_shuffle(shuffle_order.begin(), shuffle_order.end());

		while (!Q.empty() && !stop) {
			//if (debug)
			//	cout << Q.size() << " need to be derived" << endl;
			changed_bits = changed_bits_queue.front();
			changed_bits_queue.pop();
			v_ind = Q.front();
			Q.pop();
			tmp_ones_counter = ones_counter;
			for (int bit : changed_bits) {
				int tmp = 2 * (v_ind[bit] - '0') - 1;
				int l = tmp*ind[bit];
				for (int clause : clause_of_literal[l])
					tmp_ones_counter[clause] += 1;
				for (int clause : clause_of_literal[-l])
					tmp_ones_counter[clause] -= 1;
			}

			for (int j = 0; j < ind_size; j++) {
				int i = shuffle_order[j];
				bool can_derive = true;
				int tmp = 2 * (v_ind[i] - '0') - 1;
				int l = tmp*ind[i];
				for (int clause : clause_of_literal[l]) {
					if (tmp_ones_counter[clause] <= 1) {
						can_derive = false;
						break;
					}
				}


				if (can_derive) {
					new_v = v_ind;
					new_v.replace(i, 1, 1, char('0' + char(v_ind[i] == '0')));
					if (solution_set.find(new_v) == solution_set.end()) {
						//tmp_models.insert(new_v);
						new_derivation_number += 1;
                        drv_number += 1;
						output(new_v, -1, true, true);
			//			if (debug) {
			//				cout << "derived " << new_v << endl;
			//				string new_v_all = v_all;
			//				if (varset.find(ind[i]) != varset.end())
			//					new_v_all.replace(ind_to_var[i], 1, 1, new_v[i]);
			//				assert(check_answer(new_v_all));
			//			}
						if (new_derivation_number >= 10){
							//cout << "large derivation!\n";
							has_large_derivation = true;
						}
						if (new_derivation_number >= max_drv){
							stop = true;
							break;
						}

						Q.push(new_v);
						unordered_set<int> new_changed_bits = changed_bits;
						if (new_changed_bits.find(i) == new_changed_bits.end())
							new_changed_bits.insert(i);
						else
							new_changed_bits.erase(i);
						changed_bits_queue.push(new_changed_bits);
					}
					//else if (debug)
					//	cout << "derived repeated solution" << endl;
				}
			}
		}
		//clock_gettime(CLOCK_REALTIME, &end);
		//derivation_time += duration(&start, &end);
		//if (debug)
		//	cout << "end derivation" << endl;
	}

    double duration(struct timespec * a, struct timespec * b) {
        return (b->tv_sec - a->tv_sec) + 1.0e-9 * (b->tv_nsec - a->tv_nsec);
    }

	z3::expr literal(int v) {
		if (v > 0)
			return c.constant(c.str_symbol(std::to_string(v).c_str()), c.bool_sort());
		else
			return !c.constant(c.str_symbol(std::to_string(-v).c_str()), c.bool_sort());
	}
};

int main(int argc, char * argv[]) {
    int max_samples = 10000000;
    double max_time = 7200.0;
    if (argc < 2) {
        std::cout << "Argument required: input file\n";
        abort();
    }
    bool arg_samples = false;
    bool arg_time = false;
    bool enable_derive = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-n") == 0)
            arg_samples = true;
        else if (strcmp(argv[i], "-t") == 0)
            arg_time = true;
        else if (strcmp(argv[i], "-d") == 0)
            enable_derive = true;
        else if (arg_samples) {
            arg_samples = false;
            max_samples = atoi(argv[i]);
        } else if (arg_time) {
            arg_time = false;
            max_time = atof(argv[i]);
        }
    }
    QuickSampler s(argv[argc-1], max_samples, max_time, enable_derive);
    s.run();
    return 0;
}
