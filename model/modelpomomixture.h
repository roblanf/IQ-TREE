//
//  modelpomomixture.h
//  iqtree
//  Mixture PoMo models to include e.g. Gamma-rate heterogeneity
//
//  Created by Minh Bui on 7/22/16.
//
//

#ifndef modelpomomixture_h
#define modelpomomixture_h

#include <stdio.h>
#include "modelpomo.h"
#include "modelmixture.h"
#include "rateheterogeneity.h"

/**
    Mixture PoMo models
*/
class ModelPoMoMixture : public ModelPoMo, public ModelMixture {

public:
	/**
		constructor
		@param model_name model name, e.g., JC, HKY.
		@param freq state frequency type
		@param tree associated phylogenetic tree
	*/
    ModelPoMoMixture(const char *model_name,
                     string model_params,
                     StateFreqType freq_type,
                     string freq_params,
                     PhyloTree *tree,
                     bool is_reversible,
                     string pomo_params,
                     string pomo_rate_str);

    virtual ~ModelPoMoMixture();

    /** 
        save object into the checkpoint
    */
    virtual void saveCheckpoint();

    /** 
        restore object from the checkpoint
    */
    virtual void restoreCheckpoint();

	/**
		@return the number of dimensions
	*/
	virtual int getNDim();

	/**
		@return the number of dimensions corresponding to state frequencies
	*/
	virtual int getNDimFreq();
	

	/**
		the target function which needs to be optimized
		@param x the input vector x
		@return the function value at x
	*/
	virtual double targetFunk(double x[]);


	/**
	 * setup the bounds for joint optimization with BFGS
	 */
	virtual void setBounds(double *lower_bound, double *upper_bound, bool *bound_check);

	/**
		optimize model parameters
		@return the best likelihood 
	*/
	virtual double optimizeParameters(double gradient_epsilon);

	/**
		write information
		@param out output stream
	*/
	virtual void writeInfo(ostream &out);

	/**
		decompose the rate matrix into eigenvalues and eigenvectors
	*/
	virtual void decomposeRateMatrix();

    /**
     * Report the state frequencies to the output file stream 'out'.
     *
     * @param out Output file stream.
     */
    virtual void report(ostream &out);


    /**
        rate heterogeneity among sites 
    */
    RateHeterogeneity *ratehet;

protected:

    /** normally false, set to true while optimizing rate heterogeneity */
    bool optimizing_ratehet;

	/**
		this function is served for the multi-dimension optimization. It should pack the model parameters 
		into a vector that is index from 1 (NOTE: not from 0)
		@param variables (OUT) vector of variables, indexed from 1
	*/
	virtual void setVariables(double *variables);

	/**
		this function is served for the multi-dimension optimization. It should assign the model parameters 
		from a vector of variables that is index from 1 (NOTE: not from 0)
		@param variables vector of variables, indexed from 1
		@return TRUE if parameters are changed, FALSE otherwise (2015-10-20)
	*/
	virtual bool getVariables(double *variables);

};

#endif /* modelpomomixture_h */
