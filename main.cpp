#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/ml.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <map>
#include <opencv2/videoio.hpp>
#include <string>
#include <sys/signal.h>
#include <vector>
#include <fstream>
#include <ctime>
#include <thread>

#include "job.h"
#include "chromosome.h"
#include "machine.h"
#include "gantt.h"
#include "configure.h"

using namespace cv;
using namespace std;

void genetic_algorithm_initialize(
		vector<ChromosomeLinker> & linkers, 
		int size
);

Chromosome genetic_algorithm(
		vector<Chromosome> & Chromosomes, 
		vector<Job *> & Jobs, 
		map<string, Machine *> & mapMachines, 
		vector<Machine *> & vectorMachines,
		const int JOB_AMOUNT, 
		const int MACHINE_AMOUNT,
		const double INTERCROSS_RATE,
		const double MUTATION_RATE,
		const double ELITIST_RATE,
		const clock_t MAXTIME,
		map<string, vector<double> >& records
);

void progress_bar(double progress);

void crossover(const double RATE, Chromosome* parants,int parants_size, vector<Chromosome> & temp);

void mutation(const double RATE, Chromosome* parants,int parants_size, vector<Chromosome> & temp);

vector<ChromosomeLinker> RouletteWheelSelection(vector<ChromosomeLinker> & chromosomeLinker, int amounts, int nextGenerationAmount);

int random_int(int start, int end, int different_num=-1);

void constructing(map<int, Machine> & Machines, vector<Job *> & jobs, Chromosome chromosome);

vector<map<int, int> > Configure(const char * argv);


int main(int argc, const char * argv[]) {

	// usage : ./Main CHROMOSOME_AMOUNT GATIMES CROSSOVER_RATE MUTATION_RATE  ELTIST_RATE
	
	const int CHROMOSOME_AMOUNT = atoi(argv[1]);
	const double GA_TIMES = atof(argv[2]);	
	const double GA_INTERCROSS_RATE = atof(argv[3]);
	const double GA_MUTATE_RATE = atof(argv[4]);
	const double GA_ELITIST_RATE = atof(argv[5]);
	
	/* Declare Objects */ 
	vector<Job *> jobs;
	map<std::string, Machine *> mapMachines;
	vector<Machine *> vectorMachines;
	map<string, vector<double> > records;
	vector<Chromosome> Chromosomes;


	/* Computing variables
	 *
	 * */
	int i;


	/* initialization
	 * 1. load configure file
	 * 2. assign computation environment variables : 
	 * 		JOB_AMOUNT
	 * 		MACHINE_AMOUNT
	 */
	map<string, map<string, int> >Data;
	Data = EQP_TIME("./semiconductor-scheduling-data/EQP_RECIPE.txt");
	
	map<string, vector<string> >Status;
	Status = STATUS("./semiconductor-scheduling-data/Tool.txt");

	vector<map<string, string> > wipData;
	wipData = WIP("./semiconductor-scheduling-data/WIP.txt");

	vector<vector<int> > setup_time;
	setup_time = SETUP_TIME("./semiconductor-scheduling-data/Setup_time.txt");	


	for(unsigned int i = 0; i < wipData.size(); ++i){
		jobs.push_back(new Job(i, wipData[i], Data));
	}

	i = 0;
	for(map<string, vector<string> >::iterator it = Status.begin(); it != Status.end(); it++, ++i){
		mapMachines[it->first] = new Machine(i, it->first, it->second, setup_time);
		vectorMachines.push_back(mapMachines[it->first]);
	}
	srand(time(NULL));
	const int JOB_AMOUNT = jobs.size();
	const int MACHINE_AMOUNT = vectorMachines.size();
	
	Chromosome temp_chromosome;
	for(int i = 0; i < CHROMOSOME_AMOUNT; ++i){
		temp_chromosome = Chromosome(JOB_AMOUNT);
		Chromosomes.push_back(temp_chromosome);
	}


	
	Chromosome bestSolution;
	// start GA
	bestSolution = genetic_algorithm(
			Chromosomes, 
			jobs, 
			mapMachines, 
			vectorMachines,
			JOB_AMOUNT, 
			MACHINE_AMOUNT,
			GA_INTERCROSS_RATE,
			GA_MUTATE_RATE,
			GA_ELITIST_RATE,
			GA_TIMES * CLOCKS_PER_SEC,
			records
	);
	
	// reconstructing
	// machine clear
	for(map<string, Machine*>::iterator it = mapMachines.begin(); it != mapMachines.end(); it++){
		it->second->clear();
	}

	for(int j = 0; j < JOB_AMOUNT; ++j){
		jobs[j]->clear();
		jobs[j]->assign_machine_number(bestSolution.getMachine(jobs[j]->get_number()));
		jobs[j]->assign_machine_order(bestSolution.getOrder(jobs[j]->get_number()));
	}
	// Step 4. Machines get the job
	for(int j = 0; j < JOB_AMOUNT; ++j){
		mapMachines[jobs[j]->get_machine_id()]->add_job(jobs[j]);
	}
	
	int maxMachineTime = 0;
	int whichMachine;
	int temp;
	int dead, toolate;
	dead = toolate = 0;
	for(int i = 0; i < MACHINE_AMOUNT; ++i){
		vectorMachines[i]->sort_job(true);
		temp = vectorMachines[i]->get_total_time();
		dead += vectorMachines[i]->get_dead_jobs_amount();
		toolate += vectorMachines[i]->get_too_late_job_amount();
		if(temp > maxMachineTime){
			maxMachineTime = temp;
			whichMachine = i + 1;
		}
	}
	cout<<"quality = "<<records["quality"].back()<<endl;
	cout<<"makespan = "<<maxMachineTime<<endl;
	cout<<"dead = "<<dead<<endl;
	cout<<"toolate = "<<toolate<<endl;
	
	GanttChart chart(maxMachineTime, MACHINE_AMOUNT); // Gantt Chart
	for(i = 0; i < MACHINE_AMOUNT; ++i){
		vectorMachines[i]->add_into_gantt_chart(chart);
		// Machines[i].demo();
		chart.set_time(i + 1, vectorMachines[i]->get_total_time());
	}	
/*
	for(unsigned int i = 0; i < vectorMachines.size(); ++i)
		vectorMachines[i]->demo();
*/	
	
	
	// clock_t t2 = clock();
	// cout<< t2 - t1<<endl;
	// cout<<(double)(t2 - t1) / (double)CLOCKS_PER_SEC<<endl;
	

	ofstream outputFile;
	outputFile.open("outputFile.txt", ios_base::out);
	outputFile<<"x,quality,dead,toolate,makespan"<<endl;
	if(outputFile.is_open()){
		for(unsigned int i = 0; i < records["quality"].size(); ++i){
			outputFile<<i<<", "<<records["quality"][i]<<","<<records["dead"][i]<<","<<records["toolate"][i]<<"," << records["makespan"][i]<<endl;			
		}
	}

	outputFile.close();
	cout<<records["quality"].size()<<" generations"<<endl;
	
	//system("python3 test.py");
	// system("python3 plot.py");
	
	Mat mat = chart.get_img();
	namedWindow("Gantt Chart", WINDOW_AUTOSIZE );
    imshow("Gantt Chart", mat);
	waitKey(0);


	return 0;
}

void genetic_algorithm_initialize(vector<ChromosomeLinker> &linkers, int size){
	ChromosomeLinker temp;	
	for(int i = 0; i < size; ++i){
		linkers.push_back(ChromosomeLinker());
		linkers[i].link_num = i;
		linkers[i].makespan= 65535;
		linkers[i].deadJobs = 65535;
		linkers[i].tooLate = 65535;
	}
}

void progress_bar(double progress){
	printf("\033[A");
	printf("\r                                                                                                       ");	

	printf("\r[");
	for(int i = 0, max = progress - 2; i < max; i += 2){
		printf("= ");	
	}
	if(progress < 100.0)
		printf("=>");
	else
		printf("= ");
	for(int i = progress + 2; i <= 100; i += 2){
		printf("· ");
	}

	printf("]%.2f", progress);
	cout<<"%";
	printf("\n");
}


Chromosome genetic_algorithm(
		vector<Chromosome> & Chromosomes, 
		vector<Job *> & Jobs, 
		map<string, Machine *> & mapMachines, 
		vector<Machine *> & vectorMachines,
		const int JOB_AMOUNT,
		const int MACHINE_AMOUNT, 
		const double INTERCROSS_RATE,
		const double MUTATION_RATE, 
		const double ELITIST_RATE,
		const clock_t MAXTIME,
		map<string, vector<double> > & records
){
	int chromosomes_size = Chromosomes.size();
	vector<ChromosomeLinker> linkers;
	genetic_algorithm_initialize(linkers, chromosomes_size * 2);
	vector<Chromosome> temp;
	vector<Chromosome> children;
	Chromosome * chromosomes_ptr = new Chromosome[Chromosomes.size()];
	Chromosome bestSolution;
	int minTime = 65535;
	clock_t startTime, time;
	clock_t endTime = clock() + MAXTIME;
	cout<<endl;

	//init	
	for(int i = 0; i < chromosomes_size; ++i){
		chromosomes_ptr[i] = Chromosomes[i];
	}

	// init record
	records["quality"];
	records["dead"];
	records["toolate"];
	records["makespan"];
	
	for(startTime = time = clock(); time < endTime; time = clock()){
		temp.clear();
		linkers.clear();
		linkers.reserve(chromosomes_size);	
		// Step 1. crossover and mutation
		crossover(INTERCROSS_RATE, chromosomes_ptr, chromosomes_size, temp);
		mutation(MUTATION_RATE, chromosomes_ptr, chromosomes_size, temp);

		for(int i = 0; i < chromosomes_size; ++i){
			temp.push_back(chromosomes_ptr[i]);
		}

		// Step 2. assign machine number and order
		int childrenAmmount = temp.size();
		int maxDeadJobs, tempDeadJobs, maxTooLateJobs, tempTooLateJobs;
		double maxQuality, tempQuality, makeSpan, tempTotalTime;
		for(int i = 0; i < childrenAmmount;  ++i){

			// machine clear
			for(int j = 0; j < MACHINE_AMOUNT; ++j){
				vectorMachines[j]->clear();
			}

			maxQuality = tempQuality = maxDeadJobs = tempDeadJobs = maxTooLateJobs = tempTooLateJobs = makeSpan = tempTotalTime = 0;
			for(int j = 0; j < JOB_AMOUNT; ++j){
				Jobs[j]->clear();
				Jobs[j]->assign_machine_number(temp[i].getMachine(Jobs[j]->get_number()));
				Jobs[j]->assign_machine_order(temp[i].getOrder(Jobs[j]->get_number()));
			}
			// Step 4. Machines get the job
			for(int j = 0; j < JOB_AMOUNT; ++j){
				mapMachines[Jobs[j]->get_machine_id()]->add_job(Jobs[j]);
			}
			
			// sort and get maxDeadJobs, tooLateJobs, maxQuality
			for(int j = 0; j < MACHINE_AMOUNT; ++j){
				vectorMachines[j]->sort_job();
				tempQuality = vectorMachines[j]->get_quality();
				tempTotalTime = vectorMachines[j]->get_total_time();
				tempDeadJobs = vectorMachines[j]->get_dead_jobs_amount();
				tempTooLateJobs = vectorMachines[j]->get_too_late_job_amount();

				if(tempQuality > maxQuality)
					maxQuality = tempQuality;

				if(tempTooLateJobs > maxTooLateJobs)
					maxTooLateJobs = tempTooLateJobs;

				if(tempDeadJobs > maxDeadJobs)
					maxDeadJobs = tempDeadJobs;

				if(tempTotalTime > makeSpan)
					makeSpan = tempTotalTime;

			}
			ChromosomeLinker linkerTemp(i, maxQuality, maxDeadJobs, makeSpan, maxTooLateJobs , &temp[i]);
			linkers.push_back(linkerTemp);
			temp[i].value = maxQuality;
		}

		
		progress_bar(100 * ((double)(time - startTime) / (double)(endTime - startTime)));
		sort(linkers.begin(), linkers.end(), Chromosomelinker_quality_comparator);
		
		int ELITIST_AMOUNT = round(chromosomes_size * ELITIST_RATE);

		for(int i = 0; i < ELITIST_AMOUNT; ++i){
			chromosomes_ptr[i] = *linkers[i].linkChromosome;
		}
		bestSolution = chromosomes_ptr[0];
		minTime = linkers[0].quality;
		// cout<<"linker size = "<<linkers.size()<<endl;
		linkers.erase(linkers.begin() , linkers.begin() + ELITIST_AMOUNT);

		linkers = RouletteWheelSelection(linkers, childrenAmmount - ELITIST_AMOUNT, chromosomes_size - ELITIST_AMOUNT);
		// cout<<"last linker size = "<<linkers.size()<<endl;
		// cout<<"-------------------"<<endl;

		for(int j = ELITIST_AMOUNT, k = 0, size = linkers.size(); j < chromosomes_size && k < size; ++j, ++k){
			chromosomes_ptr[j] = *linkers[k].linkChromosome;
		}

		records["quality"].push_back((double)minTime);
		records["dead"].push_back((double)linkers[0].deadJobs);
		records["makespan"].push_back((double)linkers[0].makespan);
		records["toolate"].push_back((double)linkers[0].tooLate);

	}	


	progress_bar(100.0);

	delete [] chromosomes_ptr;
	return bestSolution;	
}

void crossover(const double RATE, Chromosome* parants,int parants_size,  vector<Chromosome> & temp){
	// random_shuffle(parants.begin(), parants.end());
	int newSize = round(parants_size * RATE / 2.0);
	vector<Chromosome> children;
	int num1, num2;
	for(int i = 0; i < newSize; ++i){
		num1 = random_int(0, parants_size);
		num2 = random_int(0, parants_size, num1);
		children = parants[num1] * parants[num2];
		temp.push_back(children[0]);
		temp.push_back(children[1]);
	}

	
	
}


void mutation(const double RATE, Chromosome* parants,int parants_size, vector<Chromosome> & temp){
	int newSize = round(parants_size * RATE);
	Chromosome child;
	int num1;
	for(int i = 0; i < newSize; ++i){
		num1 = random_int(0, parants_size);
		// random_shuffle(parants.begin(), parants.end());
		child = !parants[num1];
		temp.push_back(child);
	}
}

int random_int(int start, int end, int different_num){
	if(different_num < 0){
		return start + rand() % (end - start);
	}else{
		int rnd = start + (rand() % (end - start));

		while(rnd == different_num){
			rnd = start + (rand() % (end + start));
		}
		return rnd;
	}
}

vector<ChromosomeLinker> RouletteWheelSelection(vector<ChromosomeLinker> &  chromosomeLinker, int childrenAmounts, int nextGenerationAmount){
	// vector<double> values;
	double * values = new double[childrenAmounts];
	vector<ChromosomeLinker> nextGeneration;
	vector<double> randomValues;

	double total = 0.0;
	double Max = 0.0;	

	for(unsigned int i = 0, size = childrenAmounts; i < size; ++i){
		if(chromosomeLinker[i].quality > Max){
			Max = chromosomeLinker[i].quality;
		}
	}

	for(unsigned int i = 0, size = childrenAmounts; i < size; ++i){
		total += (Max - chromosomeLinker[i].quality);
	}

	for(unsigned int i = 0, size = childrenAmounts; i < size; ++i){
		values[i] = (Max - chromosomeLinker[i].quality) / total;
	}

	for(unsigned int i = 1, size =  childrenAmounts; i < size; ++i){
		values[i] += values[i - 1];
	}

	randomValues.reserve(nextGenerationAmount);	
	for(int i = 0; i < nextGenerationAmount; ++i){
		randomValues.push_back((double)rand() / (RAND_MAX + 1.0));	
	}

	nextGeneration.reserve(nextGenerationAmount);
	for(int i = 0; i < nextGenerationAmount; ++i){
		for(int j = 0; j < childrenAmounts; ++j){
			if(randomValues[i] < values[j]){ // search
				nextGeneration.push_back(chromosomeLinker[j]);
				break;
			}
		} 		
	}

	for(int i = nextGenerationAmount; i < childrenAmounts; ++i)
		nextGeneration.push_back(chromosomeLinker[i]);
	
	delete[] values;
	return nextGeneration;

}
