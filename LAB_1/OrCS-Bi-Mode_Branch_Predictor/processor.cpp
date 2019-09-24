#include "simulator.hpp"

int64_t contador_branch, contador_miss_BTB, contador_miss_BTH;

BTB_t *btb;
Predictor *predictor;
char  delay;


/// Get the next instruction from the trace
opcode_package_t *new_instruction, *next_instruction;

// If the value of BHT is ST or WT, then return true, else return false.
bool prediction(ushort entry){
	if (entry < WNTAKEN)
		return true;
	return false;
}

// Adjust global_history, direction_predictor[selected], choice predictor;
void adjust(Predictor * predict, int adjustment){

	// ORCS_PRINTF("before Global_history = %u \n", predict->global_history);

	predict->global_history = ((predict->global_history << 1) + adjustment) & 1023;
	
	// if(bk->BHT > SNTAKEN){
	// 	bk->BHT = SNTAKEN;
	// }
	// if(bk->BHT < STAKEN){
	// 	bk->BHT = STAKEN;
	// }

	// ORCS_PRINTF("after Global_history = %u \n\n", predict->global_history);

}
int verification(opcode_package_t* actual, opcode_package_t* next, Predictor* predict, ushort decision){
	int penalty;
	uint64_t NT_address;

	// Store de address of the next instruction of branch was NT.
	NT_address = actual->opcode_address + actual->opcode_size;
	
	// ORCS_PRINTF("NT_address = %" PRIu64 " \t next_address = %" PRIu64 "\n", NT_address, next->opcode_address);

	// IF BRANCH = T
	if(NT_address != next->opcode_address){
		//IF BTH = T
		if(decision == true){
			// ORCS_PRINTF("HIT (T-T)\n");
			penalty = 0;		
		}		
		//IF BTH = NT
		else{
			// ORCS_PRINTF("HIT (T-NT)\n");
			penalty = 14;
			// Counting number of misses in BHT.
			contador_miss_BTH ++;
		}

		adjust(predict, 1);
	}
	// IF BRANCH = NT
	else{
		//IF BTH = T
		if(decision == true){
			// ORCS_PRINTF("MISS (NT-T)\n");
			penalty = 14;
			// Counting number of misses in BHT.
			contador_miss_BTH ++;
		}
		//IF BTH = NT
		else{
			// ORCS_PRINTF("HIT (NT-NT)\n");
			penalty = 0;
		}	
		adjust(predict, 0);
	}

	// ORCS_PRINTF("\n\n\n");
	return penalty;
}

// =====================================================================
processor_t::processor_t() {

};

// =====================================================================
void processor_t::allocate() {
	btb = (BTB_t *) malloc(1024 * sizeof(BTB_t));
	predictor = (Predictor *) malloc(sizeof(Predictor));

	memset(btb, 0, 1024 * sizeof(BTB_t));
	memset(predictor, 0, sizeof(Predictor));

	new_instruction = (opcode_package_t *) malloc(sizeof(opcode_package_t));
	next_instruction = (opcode_package_t *) malloc(sizeof(opcode_package_t));

	predictor->direction_predictors[1] = (unsigned char *)malloc(1024 * sizeof(unsigned char));
	predictor->direction_predictors[2] = (unsigned char *)malloc(1024 * sizeof(unsigned char));
	predictor->choice_predictor = (unsigned char *)malloc(1024 * sizeof(unsigned char));

	contador_branch = 0;
	contador_miss_BTB = 0;
	contador_miss_BTH = 0;
};

// =====================================================================
void processor_t::clock() {

	uint64_t  entry, older;
	ushort selected, i;
	opcode_package_t *aux;
	bool taken;
	
	if(delay){
		delay--;
		return;	
	}
	
	//Pegando primeira instrução, no começo da execução do programa.
	if(orcs_engine.global_cycle == 0){
		next_instruction->end = orcs_engine.trace_reader->trace_fetch(next_instruction);
	}

	// Troca de onteiros de next -> new;
	aux = new_instruction;
	new_instruction = next_instruction;
	next_instruction = aux;

	//Pega próxima instrunção.
	next_instruction->end = orcs_engine.trace_reader->trace_fetch(next_instruction);

	//Caso todas as instruções tenham sido executadas.
	if (!new_instruction->end) {
		/// If EOF
		ORCS_PRINTF("CLOCKS = %" PRIu64 "\n", orcs_engine.global_cycle);
		orcs_engine.simulator_alive = false;
	}
	else{
		if(new_instruction->opcode_operation == INSTRUCTION_OPERATION_BRANCH){

			// ORCS_PRINTF("%" PRIu64 "\n", new_instruction->opcode_address);

			// Counting number of branches.
			contador_branch ++;
			entry = (new_instruction->opcode_address & 1023);
			older = 0xFFFFFFFFFFFFFFFF;

			//Trying to find the branch instruction inside the BTB.
			for(i = 0; i < WAYS; i++){
				if(btb[entry].group[i].pc == new_instruction->opcode_address){
					btb[entry].group[i].pc = new_instruction->opcode_address;
					btb[entry].group[i].time = orcs_engine.global_cycle;
					taken = prediction(i);
					delay = verification(new_instruction, next_instruction, predictor, taken);
					break;
				}
				if(older > btb[entry].group[i].time){
					older = btb[entry].group[i].time;
					selected = i;
				}
			}

			// If the branch instruction was not found into BTB.
			if(i == WAYS){
				contador_miss_BTB++;

				btb[entry].group[selected].pc = new_instruction->opcode_address;
				btb[entry].group[selected].time = orcs_engine.global_cycle;
				taken = prediction(0);
				verification(new_instruction, next_instruction, predictor, taken);

				delay = 14;

				// ORCS_PRINTF("GRUPO:\t%" PRIu64 "\n", entry);
				// ORCS_PRINTF("%" PRIu64 " \t %" PRIu64 "\n", btb[entry].group[0].pc, btb[entry].group[0].time);
				// ORCS_PRINTF("%" PRIu64 " \t %" PRIu64 "\n", btb[entry].group[1].pc, btb[entry].group[1].time);
				// ORCS_PRINTF("%" PRIu64 " \t %" PRIu64 "\n", btb[entry].group[2].pc, btb[entry].group[2].time);
				// ORCS_PRINTF("%" PRIu64 " \t %" PRIu64 "\n", btb[entry].group[3].pc, btb[entry].group[3].time);
				// ORCS_PRINTF("\n\n\n");
			}
		}
	}
};

// =====================================================================
void processor_t::statistics() {
	ORCS_PRINTF("######################################################\n");
	ORCS_PRINTF("processor_t\n");
	ORCS_PRINTF("Total Branches:%" PRIu64 "\n", contador_branch);
	ORCS_PRINTF("Total BTB misses:%" PRIu64 "\n", contador_miss_BTB);
	ORCS_PRINTF("Total BHT misses:%" PRIu64 "\n", contador_miss_BTH);
};