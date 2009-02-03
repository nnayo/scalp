#ifndef __DNA_LIST_H__
# define __DNA_LIST_H__

// all the node types
// available under the DNA component
//
typedef enum {
	DNA_SELF,	// just an mnemonic helper for the exploitation of the DNA list
	DNA_BC,		// Bus Controler (shall be unique)
	DNA_BS,		// Basic Slave
	DNA_MINUT,	// minuterie node
	DNA_XP,		// experience node
} dna_t;

#endif	// __DNA_LIST_H__
