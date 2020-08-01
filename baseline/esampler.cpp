#include <string.h>
#include <z3++.h>
#include <vector>
#include <map>
#include <stack>
#include <unordered_set>
#include <set>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <assert.h>

using namespace std;


class ESampler {
	std::string input_file;

	struct timespec start_time;
	struct timespec start, end;
	double solver_time = 0.0;
	double derivation_time = 0.0;
	int max_samples;
	int max_drv;
	double max_time;
	bool debug;
	bool random_;
	bool flip_;
	bool enable_ind;
	bool check_cnf;
	bool enumerate;
	int models_size = 0; 
	bool disable_drv;
	z3::context c;
	z3::optimize opt;
	std::vector<int> ind;
	std::vector<int> var;
	int samples = 0;
	int solver_calls = 0;

	std::ofstream results_file;
	std::ofstream count_file;

	///////////////////////////////////////
	// add variables
	///////////////////////////////////////
	unordered_set<int> backbone; 
	vector<unordered_set<int>> clauses;  // clauses with the form [(1,2,-3), (-1,2,3)]
	vector<unordered_set<int>> clauses_repeat;//same as clauses, but uesed for find backbone
	unordered_map<int, unordered_set<int>> clause_of_literal;   //  clauses set of a specific litearl
	unordered_map<int, unordered_set<int*>> clause_of_literal_repeat;   // pointer version used for propagation
	unordered_map<int, int> ind_to_var;
	unordered_map<string, int> models;  // obtained models
	vector<int*> clause_pointer_collector;
	std::unordered_set<int> varset;
	int max_var = 0;
	int run_main_count = 0;
	bool first_run_flip = true;
	int fliped_solution_num = 0, derived_solution_num = 0;
	int min_ind_count ;
	float repeat_rate = 0.0;
	bool has_large_derivation = false;
	bool has_enough_derivation = false;

public:
	ESampler(std::string input, int max_samples, double max_time, bool debug, bool random_, bool flip_, bool enable_ind, bool check_cnf, bool enumerate, int max_drv, bool disable_drv) :
	 opt(c), input_file(input), max_samples(max_samples), max_time(max_time), debug(debug), random_(random_), flip_(flip_), enable_ind(enable_ind), check_cnf(check_cnf), enumerate(enumerate), max_drv(max_drv), disable_drv(disable_drv){

	}

	///////////////////////////////////////////////////////
	// reuse run() for Algorithm 3
	//////////////////////////////////////////////////////
	void run() {
		backbone.clear();
		clauses.clear();
		clauses_repeat.clear();
		clause_of_literal.clear();
		clause_of_literal_repeat.clear();
		models.clear();
		//ind_to_var.clear();
		clause_pointer_collector.clear();
		clock_gettime(CLOCK_REALTIME, &start_time);
		srand(start_time.tv_sec);
		parse_cnf();   // Line 2-5
		string output_name = input_file + ".samples";
		min_ind_count = ind.size();

		random_? output_name+="_random":"";
		flip_? 	output_name+="_flip":"";
		enable_ind? output_name+="_ind":"";
		enumerate? output_name+="_enum":"";
		max_drv!=1000000? output_name+=(string("_drv_")+to_string(max_drv)):"";
		results_file.open(output_name);
		if (!enumerate)
			count_file.open(output_name+".repeat_count");
		if (max_drv==0)
			disable_drv==true;
		//z3::expr_vector empty(c);
		//string tmp_str = "001000001010110100000010000101000101101001000001000101110001110010010000";
		
		int unique_count = 1;
		int repeat_count = 1;
		int control_num = -1;
		int repeat_length = 0;
		//int new_solution_size = 0, old_solution_size =0;
		//double new_speed = 0.0, old_speed = 0.0, speed_rate = 0.0;
		//struct timespec lasttime;
		//struct timespec thistime;
		queue<int> solution_flow;
		//int rand_factor = 0, rand_add = 0;
		//rand_factor = ((rand() % 2) * 2 - 1);
		//std::default_random_engine rand_engine;
		//std::normal_distribution<double> nd(0, max(double(min_ind_count)/50, 0.8));
		//clock_gettime(CLOCK_REALTIME, &lasttime);
		//Xu bug: need to excluse new back bone
		if (!enumerate)
			random_ = false;

		while (true) {   // Line 7
			if(repeat_length >= 20)
				finish();
			if (debug)
				cout << "main loop " << run_main_count << endl;
			opt.push();
	/*		for (int i; i < ind.size(); i++) {
				int tmp = 2 * (tmp_str[i] - '0') - 1;
				opt.add(literal(tmp*ind[i]), 1);
			}*/
			//cout << control_num << endl;

			if(solution_flow.size() > 64){
				if (solution_flow.front())
					unique_count -=1;
				else
					repeat_count -= 1;
				solution_flow.pop();
			}

			repeat_rate = float(repeat_count)/float(unique_count);
			if (debug && run_main_count%10==0)
				cout << "repeat_rate:" << repeat_rate<<endl;
			
			if (run_main_count % 8 == 1){
				if(repeat_rate >= 0.15)
					control_num += 1;
				else if(repeat_rate <= 0.02)
					control_num -= 1;
			}
		
//			else{
//				if (run_main_count % 8 == 1){
//					clock_gettime(CLOCK_REALTIME, &thistime);
//					double elapsed = duration(&thistime, &lasttime); //neeed to add start_time!!!!!
//					new_solution_size = models.size() - old_solution_size;
//					new_speed = new_solution_size / elapsed;
//					speed_rate = new_speed / old_speed;
//					if (new_speed > 0 && speed_rate > 1)
//						rand_add += rand_factor;
//					lasttime = thistime;
//					old_solution_size = new_solution_size;
//					old_speed = new_speed;
//					rand_factor = lround(nd(rand_engine));
					//rand_factor = (rand() % 2)*2 - 1;
//					control_num += (rand_factor + rand_add);
//				}
//			}

			if(control_num < 3)
				control_num = 3;
			else if(control_num > min_ind_count)
				control_num = min_ind_count;

			int tmp_m = control_num;//randomly select m vars from ind
			for (int i = 0; i < min_ind_count; i++){
				if (!has_large_derivation && random_ && rand()%(min_ind_count - i) < tmp_m){
					opt.add(literal(((rand() %2)*2 -1)*ind[i]), 1);
					tmp_m--;
				}
				else if (!random_ || has_large_derivation)
					opt.add(literal(((rand() %2)*2 -1)*ind[i]), 1);
			}

//			for (int l : ind) {
//				if (flip_ && backbone.find(l) != backbone.end() || backbone.find(-l) != backbone.end()) continue; 
//				if (random_ && ((rand() % min_ind_count) >= control_num)) continue;
//				if (rand() % 2) {
//					opt.add(literal(l), 1);
					//tmp_str += "1";
//				}
//				else {
//					opt.add(!literal(l), 1);
					//tmp_str += "0";
//				}
				
//			}
			
			if (!solve()) {
				std::cout << "Could not find a solution!\n";
				results_file << "Could not find a solution!" << endl;
				finish();
			}
			z3::model m = opt.get_model();
			string v = model_string(m, ind);
			string v_all;
			if (!disable_drv && (has_enough_derivation || run_main_count < 3))
				v_all = model_string(m, var);
			//bool equal = (tmp_str == v);
			//if (debug && equal)
			//	cout << "bingo random guess~" << endl;
			opt.pop();
			bool unique = write_solution(v);
			if (unique || !enumerate) {
				unique_count += 1;
				repeat_length = 0;
				solution_flow.push(1);
				if (debug)
					cout << "find solution " << v << endl;
				if (flip_ && run_main_count < 1)
					flip(v, v_all);
				else if (!disable_drv && (has_enough_derivation || run_main_count < 3))
					derive(v_all, v);
				//print_stats(false);
			}
			else {
				repeat_count += 1;
				repeat_length += 1;
				solution_flow.push(0);
				if (debug)
					cout << "find repeat solution in main" << v << endl;
			}
			run_main_count++;
		}

		//print_stats(false);
		finish();
	}

	bool write_solution(string& v, bool driv=false) {

		if (models.find(v) == models.end()) {
			models[v] = 1;
			models_size += 1;
			results_file << models_size << " : " << v << endl;
			if (enumerate && models_size % 100==1){
				clock_gettime(CLOCK_REALTIME, &end);
				cout <<"time:"<< duration(&start_time, &end) << ':'<< models_size << endl;
			}
			if (driv == true)	
				derived_solution_num += 1;
			if (models_size > max_samples){
				if (driv==true){
					clock_gettime(CLOCK_REALTIME, &end);
					derivation_time += duration(&start, &end);
				}
				cout << "Stopping: samples enough\n";	
				finish();
			}
			return true;
		}
		else if(!enumerate)
		{
			models[v] += 1;
			models_size += 1;
			results_file << models_size << " : " << v << endl;
			if (driv == true)	
				derived_solution_num += 1;
			if (models_size > max_samples){
				if (driv==true){
					clock_gettime(CLOCK_REALTIME, &end);
					derivation_time += duration(&start, &end);
				}
				cout << "Stopping: samples enough\n";	
				finish();
			}
		}		
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	// Algorithm 1 with cnt
	/////////////////////////////////////////////////////////////////////////
	// int can be both positive and negative, 12 is equivalent to <12, 1>, -12 is equivalent to <12,0>
	void propagate(string v, int x) {
		if (debug)
			cout << "enter propagation" << endl;
		stack<int> B;
		
		B.push(x);
		while (!B.empty()) {
			int l = B.top();   // Line 3
			B.pop();
			for (int* p : clause_of_literal_repeat[l]) {//Line 5 Xu modified to change the value of all pointers to -1
				*p = -1;
			}
			for (int* p : clause_of_literal_repeat[-l]) {   // Line 6
				int clause_id = *p;
				if (clause_id < 0)
					continue;
				clauses_repeat[clause_id].erase(-l);
				if (clauses_repeat[clause_id].size() == 1) {  // Line 8
					*p = -1;  // Line 9
					int bb;
					for(int tmp: clauses_repeat[clause_id])
						bb = tmp;
					if (backbone.find(bb) == backbone.end()) {
						B.push(bb);  // Line 12, Fu:  we always push the variable in to B
						backbone.insert(bb);  // Line 13 to Line 16
						opt.add(literal(bb));
						if (debug)
							cout << "propagate backbone " << bb << endl;
					}
				}
			}
		}
		if (debug)
			cout << "end propagation " << endl;
	}

	///////////////////////////////////////////////////////////
	// Algorithm 2 Derivation
	//////////////////////////////////////////////////////////
	// Solutions is the parameter 'models'
	void derive(string&  v_all, string v_ind) {
		clock_gettime(CLOCK_REALTIME, &start);
		if (debug)
			cout << "enter derivation" << endl;
		queue<string> Q;
		Q.push(v_ind);
		queue<unordered_set<int>> changed_bits_queue;
		unordered_set<int> changed_bits;
		changed_bits_queue.push(changed_bits);
		unordered_map<int, int> ones_counter;
		string new_v;
		int new_derivation_number = 0;

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
		unordered_set<string> tmp_models;
		bool stop = false;

		int ind_size = ind.size();
		vector<int> shuffle_order (ind_size, 0);
		for (int k = 0; k < ind_size; k++)
			shuffle_order[k] = k;
		random_shuffle(shuffle_order.begin(), shuffle_order.end());

		while (!Q.empty() && !stop) {
			if (debug)
				cout << Q.size() << " need to be derived" << endl;
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
				if (backbone.find(l) != backbone.end())
					continue;
				for (int clause : clause_of_literal[l]) {
					if (tmp_ones_counter[clause] <= 1) {
						can_derive = false;
						break;
					}
				}


				if (can_derive) {
					new_v = v_ind;
					new_v.replace(i, 1, 1, char('0' + char(v_ind[i] == '0')));
					if (tmp_models.find(new_v) == tmp_models.end()) {
						tmp_models.insert(new_v);
						new_derivation_number += 1;
						write_solution(new_v, true);
						if (debug) {
							cout << "derived " << new_v << endl;
							string new_v_all = v_all;
							if (varset.find(ind[i]) != varset.end())
								new_v_all.replace(ind_to_var[i], 1, 1, new_v[i]);
							assert(check_answer(new_v_all));
						}
						if (new_derivation_number >= 8){
							//cout << "large derivation!\n";
							has_enough_derivation = true;
						}
						else if (new_derivation_number >= 128){
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
					else if (debug)
						cout << "derived repeated solution" << endl;
				}
			}
		}
		clock_gettime(CLOCK_REALTIME, &end);
		derivation_time += duration(&start, &end);
		if (debug)
			cout << "end derivation" << endl;
	}

	////////////////////////////////////////////
	// Algorithm 4 Flip
	///////////////////////////////////////////
	void flip(string v, string& v_all) {
		derive(v_all, v);
		if (debug)
			cout << "enter flip" << endl;	

		opt.push();
		if (flip_) {
			for (int i; i < ind.size(); i++) {
				int tmp = 2 * (v[i] - '0') - 1;
				int x = tmp * ind[i];
				opt.add(literal(x), 1);
			}
		}


		for (int i; i < ind.size(); i++) {
			int tmp = 2 * (v[i] - '0') - 1;
			int x = tmp * ind[i];
			if (backbone.find(x) != backbone.end()){
				if(first_run_flip)
					min_ind_count -= 1;
				continue;
			}
			bool solved = false;
			string new_v = v, new_v_all;
			new_v.replace(i, 1, 1, char('0' + char(v[i] == '0')));
			if (models.find(new_v) != models.end())
				continue;
			opt.push();
			opt.add(literal(-x));
			//z3::expr_vector tmp_vec(c);
			//tmp_vec.push_back(literal(-x));
			if (solved = solve()) {//give solved a value
				new_v = model_string(opt.get_model(), ind);
				new_v_all = model_string(opt.get_model(), var);
			}
			opt.pop();

			if (solved) {
				bool unique = write_solution(new_v);

				if (unique) {
					fliped_solution_num += 1;
					if (debug) {
						assert(check_answer(new_v_all));
						cout << "find flip solution at " << i << ' ' << new_v << endl;
					}
				}
				else if(debug)
					cout << "repeat flip solution at " << i << ' ' << new_v << endl;
				//Xu delete if arg_ind
				derive(new_v_all, new_v);
			}
			else if (first_run_flip) {
				if (debug)
					cout << "find backbone " << x << endl;
				min_ind_count -= 1;
				backbone.insert(x);
				opt.add(literal(x));
				propagate(v, x);
			}
		}
		opt.pop();

		if (first_run_flip) {
			for (int bb : backbone)
				opt.add(literal(bb));
			first_run_flip = false;
		}
		if (debug)
			cout << "end flip" << endl;
	}


	bool check_answer(string v_all) {
		if (debug)
			cout << "enter check_answer" << endl;

		unordered_map<int, int> ones_counter;
		for (int i = 0; i < clauses.size(); i++) {
			ones_counter[i] = 0;
		}

		for (int i = 0; i < var.size(); i++) {
			int tmp = 2 * (v_all[i] - '0') - 1;
			int l = tmp*var[i];
			for (int clause : clause_of_literal[l]) {
					ones_counter[clause] += 1;
			}
		}
		for (auto& tmp : ones_counter) {
			if (tmp.second == 0)
				return false;
		}
		return true;
	}

	void print_stats(bool simple) {
		clock_gettime(CLOCK_REALTIME, &end);
		double elapsed = duration(&start_time, &end);
		cout << "Samples: " << models.size() << endl;
		if (!enumerate)
			cout << "Repeated_samples" << models_size << endl;
		cout << "Execution time: " << elapsed <<endl;
		cout << "Time per sample: " << int(1000000*float(elapsed  / float(models.size()))) << "us" << endl;
		if (simple)
			return;
		cout << "Solver time: " << solver_time << endl;
		cout << "Solver calls: " << solver_calls << endl;
		cout << "Fliped number: " << fliped_solution_num << endl;
		cout << "Derived number: " << derived_solution_num << endl;
		if (flip_)
			cout << "Backbones number: " << backbone.size() << endl;
		cout << "repeat rate: " <<  repeat_rate << endl;
		cout << "Derivation time: " << derivation_time << endl;
	}

	void parse_cnf() {
		if (debug)
			cout << "enter parse" << endl;
		z3::expr_vector exp(c);
		std::ifstream f(input_file);
		if (!f.is_open()) {
			std::cout << "Error opening input file\n";
			abort();
		}
		std::unordered_set<int> indset;
		bool has_ind = false;
		std::string line;
		int counter = 0;
		int* clause_pointer = new int{ counter };
		clause_pointer_collector.push_back(clause_pointer);
		clause_of_literal.clear();
		clause_of_literal_repeat.clear();
		unordered_set<int> cls;
		int tmp;
		std::string tmps;
		int var_num, cls_num;
        vector<int> allInd;
		while (getline(f, line)) {
			std::istringstream iss(line);
			if (enable_ind && line.find("c ind ") == 0) {
				iss >> tmps;
				iss >> tmps;
				while (!iss.eof()) {       //// using -i to enable the independent support
					iss >> tmp;
					if (tmp && indset.find(tmp) == indset.end()) {
						indset.insert(tmp);
						allInd.push_back(tmp);
						has_ind = true;
					}
				}
			}

			else if (line[0] != 'c' && line[0] != 'p') {
				// add clauses from CNF to vector<unordered_set<int>> clauses
				if (debug && counter % 2000 == 0)
					cout << counter << " lines of cnf parsed" << endl;
				z3::expr_vector clause(c);
				while (!iss.eof()) {
					iss >> tmp;
					if (tmp != 0) {
						clause.push_back(literal(tmp));
						cls.insert(tmp);
						varset.insert(abs(tmp));//add by Xu
						if (clause_of_literal.find(tmp) == clause_of_literal.end()) {
							unordered_set<int*> clause_ids_repeat{ clause_pointer };
							unordered_set<int> clause_ids{ counter };
							clause_of_literal_repeat.insert(std::make_pair(tmp, clause_ids_repeat));
							clause_of_literal.insert(std::make_pair(tmp, clause_ids));
						}
						else {
							clause_of_literal_repeat[tmp].insert(clause_pointer);
							clause_of_literal[tmp].insert(counter);
						}
					}
					else if (clause.size() > 0) {
						counter++;//Xu modified there may be more or less than one clause in one line
						clause_pointer = new int{ counter };
						clause_pointer_collector.push_back(clause_pointer);
						exp.push_back(mk_or(clause));
						clauses.push_back(cls);
						clauses_repeat.push_back(cls);
						clause.push_back(literal(0));
						cls.clear();
					}					
				}
			}

			else if (line.find("p cnf ") == 0){
				iss >>tmps;
				iss >> tmps;
				iss>> tmp;
				var_num = tmp;
				iss >> tmp;
				cls_num = tmp;
			}

		}

		if (debug)
		{
			cout << counter << " lines of cnf parsed" << endl;
			cout << "file read end" << endl;
			cout << varset.size() << " vars and " << indset.size() << " inds and " << clauses.size() << " clauses found" <<endl;
		}
		f.close();

        int missed_num = 0;
		for(int tmp : allInd){
            if (varset.find(tmp) != varset.end())
                ind.push_back(tmp);
            else{
                missed_num += 1;
                if (check_cnf)
                    cout << tmp << endl;
            }
        }

		if (check_cnf){
            cout << "---\n";
			cout << "should   " << var_num << "   " << cls_num << endl;
			cout << "infact   " <<  varset.size() << "   " << counter << endl;
			cout << "missed " << missed_num << " inds";
			finish();
		}

		if (!enable_ind || !has_ind) {
			for (int i = 1; i <= var_num; i++)
				ind.push_back(i);
		}

		for (int lit : varset) {
			var.push_back(lit);
		}

		if (debug)
			cout << "begin make formula" << endl;
		z3::expr formula = mk_and(exp);
		opt.add(formula);
		//int ind_size = ind.size();
		//map<int, double>weight;
		//for (int i; i < ind_size; i++) {
		//	int _ = ind[i];
		//	int ones = clause_of_literal[_].size();
		//	int zeros = clause_of_literal[-_].size();
		//	double value = (ones + 1)*(zeros + 1);
		//	weight.insert(make_pair(_, value));
		//}
		//sort(ind.begin(), ind.end(), [&](int a, int b) { return weight[a]>=weight[b]; });

		if (debug) {
			cout << "enter ind to var process" << endl;
			for (int i = 0; i < ind.size(); i++) {
				int tmp = ind[i];
				for (int j = 0; j < var.size(); j++) {
					if (var[j] == tmp)
						ind_to_var.insert(make_pair(i, j));
				}
			}
			cout << "out ind to var process and end parse" << endl;
		}
	}

	void finish() {
        if (!check_cnf){
		    print_stats(false);
        }

		for (int* p : clause_pointer_collector)
			delete p;
		results_file.close();
		if (!enumerate){
			unordered_map<int, int> counter;
			for (auto & v :models){
				if (counter.find(v.second) == counter.end())
					counter[v.second] = 1;
				else
					counter[v.second] += 1;
			}
			for (auto & v: counter){
				count_file << v.first << ' ' <<  v.second << endl;
			}
		}
		count_file.close();
		exit(0);
	}

	bool solve() {

		clock_gettime(CLOCK_REALTIME, &start);
		double elapsed = duration(&start_time, &start);
		if (elapsed > max_time) {
			std::cout << "Stopping: timeout\n";
			finish();
		}
		usleep(1);
		z3::check_result result = opt.check();
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
	double max_time = 72000.0;     
	int max_drv = 10000;

	if (argc < 2) {
		std::cout << "Argument required: input file\n";
		abort();
	}
	bool arg_samples = false;
	bool arg_time = false; 
	bool arg_debug = false;
	bool arg_random = false;
	bool arg_flip = false;
	bool arg_enable_ind = false;
	bool arg_check_cnf = false;
	bool arg_enumerate = false;
	bool arg_drv = false;
	bool arg_nd = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-n") == 0)
			arg_samples = true;
		else if (strcmp(argv[i], "-t") == 0)
			arg_time = true;
		else if (strcmp(argv[i], "-d") == 0)
			arg_debug = true;
		else if (strcmp(argv[i], "-r") == 0)
			arg_random = true;
		else if (strcmp(argv[i], "-f") == 0)
			arg_flip = true;
		else if (strcmp(argv[i], "-i") == 0)
			arg_enable_ind = true;
		else if (strcmp(argv[i], "-c") == 0)
			arg_check_cnf = true;
		else if (strcmp(argv[i], "-e") == 0)
			arg_enumerate = true;
		else if (strcmp(argv[i], "--drv") == 0)
			arg_drv = true;
		else if (strcmp(argv[i], "-nd") == 0)
			arg_nd = true;
		else if (arg_samples) {
			arg_samples = false;
			max_samples = atoi(argv[i]);
		}
		else if (arg_time) {
			arg_time = false;
			max_time = atof(argv[i]);
		}
		else if (arg_drv) {
			arg_drv = false;
			max_drv = atoi(argv[i]);
		}

	}
	ESampler s(argv[argc - 1], max_samples, max_time, arg_debug, arg_random, arg_flip, arg_enable_ind, arg_check_cnf, arg_enumerate, max_drv, arg_nd);
	s.run();
	return 0;
}









































