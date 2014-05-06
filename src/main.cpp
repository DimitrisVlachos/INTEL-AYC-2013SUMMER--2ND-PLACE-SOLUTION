
/*
	Original Code : Intel Software (Accelerate your code contest 2013)
	Optimized Code :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#include "types.hpp"
#include "mt.hpp"
#include "solvers.hpp"



bool read_parameters(int32_t argc, char* argv[],parameters_t& parameters);
void output_list(std::vector<result_t>& result_list);

static inline bool compare_results(const result_t& first,const result_t& second);

int main(int argc, char* argv[]) {

	if (argv == NULL) {
		std::cout<<"??argv == NULL?"<<std::endl;
		return 0;
	}

	parameters_t parameters;
	std::vector<result_t> result_list;

	if (!read_parameters(argc, argv, parameters)) {
		std::cout<<"Wrong number of parameters or invalid parameters..."<<std::endl;
		std::cout<<"The program must be called with the following parameters:"<<std::endl;
		std::cout<<"\t- num_threads: The number of threads"<<std::endl;
		std::cout<<"\t- max_scale: The maximum scale that can be applied to the templates in the main image"<<std::endl;
		std::cout<<"\t- main_image: The main image path"<<std::endl;
		std::cout<<"\t- t1 ... tn: The list of the template paths. Each template separated by a space"<<std::endl;
		std::cout<<std::endl<<"For example : ./run 4 3 img.bmp template1.bmp template2.bmp"<<std::endl;
		return -1;
	}
	mt_init(parameters.nb_threads);
		solve_mt(parameters,result_list);
		output_list(result_list);
	mt_shutdown();
	return 0;
}

void output_list(std::vector<result_t>& result_list) {
	std::sort(result_list.begin(),result_list.end(),compare_results);
	for (const result_t& res : result_list)
		std::cout<<res.pattern<<"\t"<<res.px<<"\t"<<res.py<<std::endl;
}

bool read_parameters(int32_t argc, char* argv[],parameters_t& parameters){
	if (argc < 4) {
		return false;
	}

	parameters.nb_threads = atoi(argv[1]);
	if(parameters.nb_threads < 0) {
		return false;
	}

	parameters.max_scale = atoi(argv[2]);
	if(parameters.max_scale <= 0) {
		return false;
	}

	parameters.main_image_name = std::string(argv[3]);

	for(int32_t i=4; i<argc; i++){
		parameters.template_names.push_back(std::string(argv[i]));
	}
	
	return true;	
}


static inline bool compare_results(const result_t& first,const result_t& second) {
	if(first.pattern < second.pattern) { return true; }
	else if(first.pattern > second.pattern) { return false; }

	if(first.px < second.px) { return true; }
	else if(first.px > second.px) { return false; }

	if(first.py < second.py) { return true; }
	else if(first.py > second.py) { return false; }

	return true;	
}



