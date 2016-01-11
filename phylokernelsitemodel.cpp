/*
 * phylokernelsitemodel.cpp
 * likelihood kernel site-specific frequency model
 *
 *  Created on: Jan 9, 2016
 *      Author: minh
 */



#include "phylotree.h"
#include "model/modelset.h"

void PhyloTree::computeSitemodelPartialLikelihoodEigen(PhyloNeighbor *dad_branch, PhyloNode *dad) {

    // don't recompute the likelihood
	assert(dad);
    if (dad_branch->partial_lh_computed & 1)
        return;
    dad_branch->partial_lh_computed |= 1;
    PhyloNode *node = (PhyloNode*)(dad_branch->node);


    size_t nstates = aln->num_states;
    size_t nptn = aln->size()+model_factory->unobserved_ptns.size();
    size_t ptn, c;
//    size_t orig_ntn = aln->size();
    size_t ncat = site_rate->getNRate();
//    const size_t nstatesqr=nstates*nstates;
    size_t i, x;
    size_t block = nstates * ncat;


	if (node->isLeaf()) {
	    dad_branch->lh_scale_factor = 0.0;
		// scale number must be ZERO
	    memset(dad_branch->scale_num, 0, nptn * sizeof(UBYTE));

		if (!tip_partial_lh_computed)
			computeTipPartialLikelihood();


        ModelSet *models = (ModelSet*) model;
        assert(models->size() == nptn);
        
        for (ptn = 0; ptn < nptn; ptn++) {
            int state = aln->at(ptn)[node->id];
            double *partial_lh = dad_branch->partial_lh + ptn*block;
            double *inv_evec = models->at(ptn)->getInverseEigenvectors();

            if (state < nstates) {
                for (i = 0; i < nstates; i++)
                    partial_lh[i] = inv_evec[i*nstates+state];
            } else if (state == aln->STATE_UNKNOWN) {
                // special treatment for unknown char
                for (i = 0; i < nstates; i++) {
                    double lh_unknown = 0.0;
                    double *this_inv_evec = inv_evec + i*nstates;
                    for (x = 0; x < nstates; x++)
                        lh_unknown += this_inv_evec[x];
                    partial_lh[i] = lh_unknown;
                }
            } else {
                double lh_ambiguous;
                // ambiguous characters
                int ambi_aa[] = {
                    4+8, // B = N or D
                    32+64, // Z = Q or E
                    512+1024 // U = I or L
                    };
                switch (aln->seq_type) {
                case SEQ_DNA:
                    {
                        int cstate = state-nstates+1;
                        for (i = 0; i < nstates; i++) {
                            lh_ambiguous = 0.0;
                            for (x = 0; x < nstates; x++)
                                if ((cstate) & (1 << x))
                                    lh_ambiguous += inv_evec[i*nstates+x];
                            partial_lh[i] = lh_ambiguous;
                        }
                    }
                    break;
                case SEQ_PROTEIN:
                    //map[(unsigned char)'B'] = 4+8+19; // N or D
                    //map[(unsigned char)'Z'] = 32+64+19; // Q or E
                    {
                        for (i = 0; i < nstates; i++) {
                            lh_ambiguous = 0.0;
                            for (x = 0; x < 11; x++)
                                if (ambi_aa[state] & (1 << x))
                                    lh_ambiguous += inv_evec[i*nstates+x];
                            partial_lh[i] = lh_ambiguous;
                        }
                    }
                    break;
                default:
                    assert(0);
                    break;
                }
            }
            
            // copy partial_lh for all categories
            for (c = 1; c < ncat; c++)
                memcpy(partial_lh+c*nstates, partial_lh, nstates*sizeof(double));
        }
            
		return;
	}
    
//	double *evec = model->getEigenvectors();
//	double *inv_evec = model->getInverseEigenvectors();
//	assert(inv_evec && evec);
//	double *eval = model->getEigenvalues();

    dad_branch->lh_scale_factor = 0.0;

	// internal node
	PhyloNeighbor *left = NULL, *right = NULL; // left & right are two neighbors leading to 2 subtrees
	FOR_NEIGHBOR_IT(node, dad, it) {
        PhyloNeighbor *nei = (PhyloNeighbor*)*it;
		if (!left) left = (PhyloNeighbor*)(*it); else right = (PhyloNeighbor*)(*it);
        if ((nei->partial_lh_computed & 1) == 0)
            computePartialLikelihood(nei, node);
        dad_branch->lh_scale_factor += nei->lh_scale_factor;
	}

    if (params->lh_mem_save == LM_PER_NODE && !dad_branch->partial_lh) {
        // re-orient partial_lh
        bool done = false;
        FOR_NEIGHBOR_IT(node, dad, it2) {
            PhyloNeighbor *backnei = ((PhyloNeighbor*)(*it2)->node->findNeighbor(node));
            if (backnei->partial_lh) {
                dad_branch->partial_lh = backnei->partial_lh;
                dad_branch->scale_num = backnei->scale_num;
                backnei->partial_lh = NULL;
                backnei->scale_num = NULL;
                backnei->partial_lh_computed &= ~1; // clear bit
                done = true;
                break;
            }
        }
        assert(done && "partial_lh is not re-oriented");
    }

    // precompute buffer to save times
//    double *echildren = new double[block*nstates*(node->degree()-1)];
//    double *partial_lh_leaves = new double[(aln->STATE_UNKNOWN+1)*block*(node->degree()-1)];
//    double *echild = echildren;
//    double *partial_lh_leaf = partial_lh_leaves;
//
//    FOR_NEIGHBOR_IT(node, dad, it) {
//        double expchild[nstates];
//        PhyloNeighbor *child = (PhyloNeighbor*)*it;
//        // precompute information buffer
//        for (c = 0; c < ncat; c++) {
//            double len_child = site_rate->getRate(c) * child->length;
//            for (i = 0; i < nstates; i++) {
//                expchild[i] = exp(eval[i]*len_child);
//            }
//            for (x = 0; x < nstates; x++)
//                for (i = 0; i < nstates; i++) {
//                    echild[c*nstatesqr+x*nstates+i] = evec[x*nstates+i] * expchild[i];
//                }
//        }
//
//        // pre compute information for tip
//        if (child->node->isLeaf()) {
//            vector<int>::iterator it;
//            for (it = aln->seq_states[child->node->id].begin(); it != aln->seq_states[child->node->id].end(); it++) {
//                int state = (*it);
//                for (x = 0; x < block; x++) {
//                    double vchild = 0.0;
//                    for (i = 0; i < nstates; i++) {
//                        vchild += echild[x*nstates+i] * tip_partial_lh[state*nstates+i];
//                    }
//                    partial_lh_leaf[state*block+x] = vchild;
//                }
//            }
//            for (x = 0; x < block; x++) {
//                size_t addr = aln->STATE_UNKNOWN * block;
//                partial_lh_leaf[addr+x] = 1.0;
//            }
//            partial_lh_leaf += (aln->STATE_UNKNOWN+1)*block;
//        }
//        echild += block*nstates;
//    }
    
    
    double sum_scale = 0.0;
    
        
//    double *eleft = echildren, *eright = echildren + block*nstates;
    
	if (!left->node->isLeaf() && right->node->isLeaf()) {
		PhyloNeighbor *tmp = left;
		left = right;
		right = tmp;
//        double *etmp = eleft;
//        eleft = eright;
//        eright = etmp;
	}
    
//    if (node->degree() > 3) {
//
//        /*--------------------- multifurcating node ------------------*/
//    
//        // now for-loop computing partial_lh over all site-patterns
//#ifdef _OPENMP
//#pragma omp parallel for reduction(+: sum_scale) private(ptn, c, x, i) schedule(static)
//#endif
//        for (ptn = 0; ptn < nptn; ptn++) {
//            double partial_lh_all[block];
//            for (i = 0; i < block; i++)
//                partial_lh_all[i] = 1.0;
//            dad_branch->scale_num[ptn] = 0;
//                
//            double *partial_lh_leaf = partial_lh_leaves;
//            double *echild = echildren;
//
//            FOR_NEIGHBOR_IT(node, dad, it) {
//                PhyloNeighbor *child = (PhyloNeighbor*)*it;
//                if (child->node->isLeaf()) {
//                    // external node
//                    int state_child = (ptn < orig_ntn) ? (aln->at(ptn))[child->node->id] : model_factory->unobserved_ptns[ptn-orig_ntn];
//                    double *child_lh = partial_lh_leaf + state_child*block;
//                    for (c = 0; c < block; c++) {
//                        // compute real partial likelihood vector
//                        partial_lh_all[c] *= child_lh[c];
//                    }
//                    partial_lh_leaf += (aln->STATE_UNKNOWN+1)*block;
//                } else {
//                    // internal node
//                    double *partial_lh = partial_lh_all;
//                    double *partial_lh_child = child->partial_lh + ptn*block;
//                    dad_branch->scale_num[ptn] += child->scale_num[ptn];
//
//                    double *echild_ptr = echild;
//                    for (c = 0; c < ncat; c++) {
//                        // compute real partial likelihood vector
//                        for (x = 0; x < nstates; x++) {
//                            double vchild = 0.0;
////                            double *echild_ptr = echild + (c*nstatesqr+x*nstates);
//                            for (i = 0; i < nstates; i++) {
//                                vchild += echild_ptr[i] * partial_lh_child[i];
//                            }
//                            echild_ptr += nstates;
//                            partial_lh[x] *= vchild;
//                        }
//                        partial_lh += nstates;
//                        partial_lh_child += nstates;
//                    }
//                } // if
//                echild += block*nstates;
//            } // FOR_NEIGHBOR
//            
//        
//            // compute dot-product with inv_eigenvector
//            double lh_max = 0.0;
//            double *partial_lh_tmp = partial_lh_all;
//            double *partial_lh = dad_branch->partial_lh + ptn*block;
//            for (c = 0; c < ncat; c++) {
//                double *inv_evec_ptr = inv_evec;
//                for (i = 0; i < nstates; i++) {
//                    double res = 0.0;
//                    for (x = 0; x < nstates; x++) {
//                        res += partial_lh_tmp[x]*inv_evec_ptr[x];
//                    }
//                    inv_evec_ptr += nstates;
//                    partial_lh[i] = res;
//                    lh_max = max(lh_max, fabs(res));
//                }
//                partial_lh += nstates;
//                partial_lh_tmp += nstates;
//            }
//            // check if one should scale partial likelihoods
//            if (lh_max < SCALING_THRESHOLD) {
//                partial_lh = dad_branch->partial_lh + ptn*block;
//                if (lh_max == 0.0) {
//                    // for very shitty data
//                    for (c = 0; c < ncat; c++)
//                        memcpy(&partial_lh[c*nstates], &tip_partial_lh[aln->STATE_UNKNOWN*nstates], nstates*sizeof(double));
//                    sum_scale += LOG_SCALING_THRESHOLD* 4 * ptn_freq[ptn];
//                    //sum_scale += log(lh_max) * ptn_freq[ptn];
//                    dad_branch->scale_num[ptn] += 4;
//                    int nsite = aln->getNSite();
//                    for (i = 0, x = 0; i < nsite && x < ptn_freq[ptn]; i++)
//                        if (aln->getPatternID(i) == ptn) {
//                            outWarning((string)"Numerical underflow for site " + convertIntToString(i+1));
//                            x++;
//                        }
//                } else {
//                    // now do the likelihood scaling
//                    for (i = 0; i < block; i++) {
//                        partial_lh[i] *= SCALING_THRESHOLD_INVER;
//                        //partial_lh[i] /= lh_max;
//                    }
//                    // unobserved const pattern will never have underflow
//                    sum_scale += LOG_SCALING_THRESHOLD * ptn_freq[ptn];
//                    //sum_scale += log(lh_max) * ptn_freq[ptn];
//                    dad_branch->scale_num[ptn] += 1;
//                }
//            }
//
//        } // for ptn
//        dad_branch->lh_scale_factor += sum_scale;               
//                
//        // end multifurcating treatment
//    } else if (left->node->isLeaf() && right->node->isLeaf()) {
//
//        /*--------------------- TIP-TIP (cherry) case ------------------*/
//
//        double *partial_lh_left = partial_lh_leaves;
//        double *partial_lh_right = partial_lh_leaves + (aln->STATE_UNKNOWN+1)*block;
//
//		// scale number must be ZERO
//	    memset(dad_branch->scale_num, 0, nptn * sizeof(UBYTE));
//#ifdef _OPENMP
//#pragma omp parallel for private(ptn, c, x, i) schedule(static)
//#endif
//		for (ptn = 0; ptn < nptn; ptn++) {
//			double partial_lh_tmp[nstates];
//			double *partial_lh = dad_branch->partial_lh + ptn*block;
//			int state_left = (ptn < orig_ntn) ? (aln->at(ptn))[left->node->id] : model_factory->unobserved_ptns[ptn-orig_ntn];
//			int state_right = (ptn < orig_ntn) ? (aln->at(ptn))[right->node->id] : model_factory->unobserved_ptns[ptn-orig_ntn];
//			for (c = 0; c < ncat; c++) {
//				// compute real partial likelihood vector
//				double *left = partial_lh_left + (state_left*block+c*nstates);
//				double *right = partial_lh_right + (state_right*block+c*nstates);
//				for (x = 0; x < nstates; x++) {
//					partial_lh_tmp[x] = left[x] * right[x];
//				}
//
//				// compute dot-product with inv_eigenvector
//                double *inv_evec_ptr = inv_evec;
//				for (i = 0; i < nstates; i++) {
//					double res = 0.0;
//					for (x = 0; x < nstates; x++) {
//						res += partial_lh_tmp[x]*inv_evec_ptr[x];
//					}
//                    inv_evec_ptr += nstates;
//					partial_lh[c*nstates+i] = res;
//				}
//			}
//		}
//	} else if (left->node->isLeaf() && !right->node->isLeaf()) {
//
//        /*--------------------- TIP-INTERNAL NODE case ------------------*/
//
//		// only take scale_num from the right subtree
//		memcpy(dad_branch->scale_num, right->scale_num, nptn * sizeof(UBYTE));
//
//
//        double *partial_lh_left = partial_lh_leaves;
//
//#ifdef _OPENMP
//#pragma omp parallel for reduction(+: sum_scale) private(ptn, c, x, i) schedule(static)
//#endif
//		for (ptn = 0; ptn < nptn; ptn++) {
//			double partial_lh_tmp[nstates];
//			double *partial_lh = dad_branch->partial_lh + ptn*block;
//			double *partial_lh_right = right->partial_lh + ptn*block;
//			int state_left = (ptn < orig_ntn) ? (aln->at(ptn))[left->node->id] : model_factory->unobserved_ptns[ptn-orig_ntn];
//            double *vleft = partial_lh_left + state_left*block;
//            double lh_max = 0.0;
//            
//            double *eright_ptr = eright;
//			for (c = 0; c < ncat; c++) {
//				// compute real partial likelihood vector
//				for (x = 0; x < nstates; x++) {
//					double vright = 0.0;
////					size_t addr = c*nstatesqr+x*nstates;
////					vleft = partial_lh_left[state_left*block+c*nstates+x];
//					for (i = 0; i < nstates; i++) {
//						vright += eright_ptr[i] * partial_lh_right[i];
//					}
//                    eright_ptr += nstates;
//					partial_lh_tmp[x] = vleft[x] * (vright);
//				}
//                vleft += nstates;
//                partial_lh_right += nstates;
//                
//				// compute dot-product with inv_eigenvector
//                double *inv_evec_ptr = inv_evec;
//				for (i = 0; i < nstates; i++) {
//					double res = 0.0;
//					for (x = 0; x < nstates; x++) {
//						res += partial_lh_tmp[x]*inv_evec_ptr[x];
//					}
//                    inv_evec_ptr += nstates;
//					partial_lh[c*nstates+i] = res;
//                    lh_max = max(fabs(res), lh_max);
//				}
//			}
//            // check if one should scale partial likelihoods
//            if (lh_max < SCALING_THRESHOLD) {
//            	if (lh_max == 0.0) {
//            		// for very shitty data
//            		for (c = 0; c < ncat; c++)
//            			memcpy(&partial_lh[c*nstates], &tip_partial_lh[aln->STATE_UNKNOWN*nstates], nstates*sizeof(double));
//					sum_scale += LOG_SCALING_THRESHOLD* 4 * ptn_freq[ptn];
//					//sum_scale += log(lh_max) * ptn_freq[ptn];
//					dad_branch->scale_num[ptn] += 4;
//					int nsite = aln->getNSite();
//					for (i = 0, x = 0; i < nsite && x < ptn_freq[ptn]; i++)
//						if (aln->getPatternID(i) == ptn) {
//							outWarning((string)"Numerical underflow for site " + convertIntToString(i+1));
//							x++;
//						}
//            	} else {
//					// now do the likelihood scaling
//					for (i = 0; i < block; i++) {
//						partial_lh[i] *= SCALING_THRESHOLD_INVER;
//	                    //partial_lh[i] /= lh_max;
//					}
//					// unobserved const pattern will never have underflow
//					sum_scale += LOG_SCALING_THRESHOLD * ptn_freq[ptn];
//					//sum_scale += log(lh_max) * ptn_freq[ptn];
//					dad_branch->scale_num[ptn] += 1;
//            	}
//            }
//
//
//		}
//		dad_branch->lh_scale_factor += sum_scale;
////		delete [] partial_lh_left;
//
//	} else {

    {
        /*--------------------- INTERNAL-INTERNAL NODE case ------------------*/

        ModelSet *models = (ModelSet*)model;

#ifdef _OPENMP
#pragma omp parallel for reduction(+: sum_scale) private(ptn, c, x, i) schedule(static)
#endif
		for (ptn = 0; ptn < nptn; ptn++) {
			double partial_lh_tmp[nstates];
			double *partial_lh = dad_branch->partial_lh + ptn*block;
			double *partial_lh_left = left->partial_lh + ptn*block;
			double *partial_lh_right = right->partial_lh + ptn*block;
            double lh_max = 0.0;

            double expleft[nstates];
            double expright[nstates];
            double *eval = models->at(ptn)->getEigenvalues();
            double *evec = models->at(ptn)->getEigenvectors();
            double *inv_evec = models->at(ptn)->getInverseEigenvectors();

			dad_branch->scale_num[ptn] = left->scale_num[ptn] + right->scale_num[ptn];

			for (c = 0; c < ncat; c++) {
                double len_left = site_rate->getRate(c) * left->length;
                double len_right = site_rate->getRate(c) * right->length;
                for (i = 0; i < nstates; i++) {
                    expleft[i] = exp(eval[i]*len_left);
                    expright[i] = exp(eval[i]*len_right);
                }
				// compute real partial likelihood vector
				for (x = 0; x < nstates; x++) {
					double vleft = 0.0, vright = 0.0;
                    double *this_evec = evec + x*nstates;
					for (i = 0; i < nstates; i++) {
						vleft += this_evec[i] * expleft[i] * partial_lh_left[i];
						vright += this_evec[i] * expright[i] * partial_lh_right[i];
					}
					partial_lh_tmp[x] = vleft*vright;
				}
                partial_lh_left += nstates;
                partial_lh_right += nstates;
                
				// compute dot-product with inv_eigenvector
                double *inv_evec_ptr = inv_evec;
				for (i = 0; i < nstates; i++) {
					double res = 0.0;
					for (x = 0; x < nstates; x++) {
						res += partial_lh_tmp[x]*inv_evec_ptr[x];
					}
                    inv_evec_ptr += nstates;
					partial_lh[c*nstates+i] = res;
                    lh_max = max(lh_max, fabs(res));
				}
			}

            // check if one should scale partial likelihoods
            if (lh_max < SCALING_THRESHOLD) {
            	if (lh_max == 0.0) {
            		// for very shitty data
            		for (c = 0; c < ncat; c++)
            			memcpy(&partial_lh[c*nstates], &tip_partial_lh[aln->STATE_UNKNOWN*nstates], nstates*sizeof(double));
					sum_scale += LOG_SCALING_THRESHOLD* 4 * ptn_freq[ptn];
					//sum_scale += log(lh_max) * ptn_freq[ptn];
					dad_branch->scale_num[ptn] += 4;
					int nsite = aln->getNSite();
					for (i = 0, x = 0; i < nsite && x < ptn_freq[ptn]; i++)
						if (aln->getPatternID(i) == ptn) {
							outWarning((string)"Numerical underflow for site " + convertIntToString(i+1));
							x++;
						}
            	} else {
					// now do the likelihood scaling
					for (i = 0; i < block; i++) {
						partial_lh[i] *= SCALING_THRESHOLD_INVER;
	                    //partial_lh[i] /= lh_max;
					}
					// unobserved const pattern will never have underflow
					sum_scale += LOG_SCALING_THRESHOLD * ptn_freq[ptn];
					//sum_scale += log(lh_max) * ptn_freq[ptn];
					dad_branch->scale_num[ptn] += 1;
            	}
            }

		}
		dad_branch->lh_scale_factor += sum_scale;

	}

//    delete [] partial_lh_leaves;
//    delete [] echildren;
}

//template <const int nstates>
void PhyloTree::computeSitemodelLikelihoodDervEigen(PhyloNeighbor *dad_branch, PhyloNode *dad, double &df, double &ddf) {
    PhyloNode *node = (PhyloNode*) dad_branch->node;
    PhyloNeighbor *node_branch = (PhyloNeighbor*) node->findNeighbor(dad);
    if (!central_partial_lh)
        initializeAllPartialLh();
    if (node->isLeaf()) {
    	PhyloNode *tmp_node = dad;
    	dad = node;
    	node = tmp_node;
    	PhyloNeighbor *tmp_nei = dad_branch;
    	dad_branch = node_branch;
    	node_branch = tmp_nei;
    }
    if ((dad_branch->partial_lh_computed & 1) == 0)
        computePartialLikelihood(dad_branch, dad);
    if ((node_branch->partial_lh_computed & 1) == 0)
        computePartialLikelihood(node_branch, node);
        
    size_t nstates = aln->num_states;
    size_t ncat = site_rate->getNRate();

    size_t block = ncat * nstates;
    size_t ptn; // for big data size > 4GB memory required
    size_t c, i;
//    size_t orig_nptn = aln->size();
    size_t nptn = aln->size()+model_factory->unobserved_ptns.size();
//    double *eval = model->getEigenvalues();
//    assert(eval);

	assert(theta_all);
	if (!theta_computed) {
		// precompute theta for fast branch length optimization

//	    if (dad->isLeaf()) {
//	    	// special treatment for TIP-INTERNAL NODE case
//#ifdef _OPENMP
//#pragma omp parallel for private(ptn, i) schedule(static)
//#endif
//	    	for (ptn = 0; ptn < nptn; ptn++) {
//				double *partial_lh_dad = dad_branch->partial_lh + ptn*block;
//				double *theta = theta_all + ptn*block;
//				double *lh_tip = tip_partial_lh + ((int)((ptn < orig_nptn) ? (aln->at(ptn))[dad->id] :  model_factory->unobserved_ptns[ptn-orig_nptn]))*nstates;
//                for (c = 0; c < ncat; c++) {
//                    for (i = 0; i < nstates; i++) {
//                        theta[i] = lh_tip[i] * partial_lh_dad[i];
//                    }
//                    partial_lh_dad += nstates;
//                    theta += nstates;
//                }
//
//			}
//			// ascertainment bias correction
//	    } else 
        {
	    	// both dad and node are internal nodes

//	    	size_t all_entries = nptn*block;
#ifdef _OPENMP
#pragma omp parallel for private(ptn, i) schedule(static)
#endif
	    	for (ptn = 0; ptn < nptn; ptn++) {
				double *theta = theta_all + ptn*block;
			    double *partial_lh_node = node_branch->partial_lh + ptn*block;
			    double *partial_lh_dad = dad_branch->partial_lh + ptn*block;
	    		for (i = 0; i < block; i++) {
	    			theta[i] = partial_lh_node[i] * partial_lh_dad[i];
	    		}
			}
	    }
		theta_computed = true;
	}

    double my_df = 0.0, my_ddf = 0.0;
//    double tree_lh = node_branch->lh_scale_factor + dad_branch->lh_scale_factor;

#ifdef _OPENMP
#pragma omp parallel for reduction(+: my_df, my_ddf, prob_const, df_const, ddf_const) private(ptn, i) schedule(static)
#endif
    for (ptn = 0; ptn < nptn; ptn++) {
		double lh_ptn = ptn_invar[ptn], df_ptn = 0.0, ddf_ptn = 0.0;
		double *theta = theta_all + ptn*block;
//        double val0[block];
//        double val1[block];
//        double val2[block];
        
        ModelSet *models = (ModelSet*)model;
        double *eval = models->at(ptn)->getEigenvalues();
        
        for (c = 0; c < ncat; c++) {
            double prop = site_rate->getProp(c);
            for (i = 0; i < nstates; i++) {
                double cof = eval[i]*site_rate->getRate(c);
                double val = exp(cof*dad_branch->length) * prop;
                double val1_ = cof*val;
                double val2_ = cof*val1_;
//                val0[c*nstates+i] = val;
//                val1[c*nstates+i] = val1_;
//                val2[c*nstates+i] = cof*val1_;
                lh_ptn += val * theta[i];
                df_ptn += val1_ * theta[i];
                ddf_ptn += val2_ * theta[i];
            }
        }


//		for (i = 0; i < block; i++) {
//			lh_ptn += val0[i] * theta[i];
//			df_ptn += val1[i] * theta[i];
//			ddf_ptn += val2[i] * theta[i];
//		}

//        assert(lh_ptn > 0.0);
        lh_ptn = fabs(lh_ptn);
        
//        if (ptn < orig_nptn) 
        {
			double df_frac = df_ptn / lh_ptn;
			double ddf_frac = ddf_ptn / lh_ptn;
			double freq = ptn_freq[ptn];
			double tmp1 = df_frac * freq;
			double tmp2 = ddf_frac * freq;
			my_df += tmp1;
			my_ddf += tmp2 - tmp1 * df_frac;
		}
//         else {
//			// ascertainment bias correction
//			prob_const += lh_ptn;
//			df_const += df_ptn;
//			ddf_const += ddf_ptn;
//		}
    }
	df = my_df;
	ddf = my_ddf;
    if (isnan(df) || isinf(df)) {
        df = 0.0;
        ddf = 0.0;
//        outWarning("Numerical instability (some site-likelihood = 0)");
    }

//	if (orig_nptn < nptn) {
//    	// ascertainment bias correction
//    	prob_const = 1.0 - prob_const;
//    	double df_frac = df_const / prob_const;
//    	double ddf_frac = ddf_const / prob_const;
//    	int nsites = aln->getNSite();
//    	df += nsites * df_frac;
//    	ddf += nsites *(ddf_frac + df_frac*df_frac);
//    }


//    delete [] val2;
//    delete [] val1;
//    delete [] val0;
}

//template <const int nstates>
double PhyloTree::computeSitemodelLikelihoodBranchEigen(PhyloNeighbor *dad_branch, PhyloNode *dad) {
    PhyloNode *node = (PhyloNode*) dad_branch->node;
    PhyloNeighbor *node_branch = (PhyloNeighbor*) node->findNeighbor(dad);
    if (!central_partial_lh)
        initializeAllPartialLh();
    if (node->isLeaf()) {
    	PhyloNode *tmp_node = dad;
    	dad = node;
    	node = tmp_node;
    	PhyloNeighbor *tmp_nei = dad_branch;
    	dad_branch = node_branch;
    	node_branch = tmp_nei;
    }
    if ((dad_branch->partial_lh_computed & 1) == 0)
//        computePartialLikelihoodEigen(dad_branch, dad);
        computePartialLikelihood(dad_branch, dad);
    if ((node_branch->partial_lh_computed & 1) == 0)
//        computePartialLikelihoodEigen(node_branch, node);
        computePartialLikelihood(node_branch, node);
    double tree_lh = node_branch->lh_scale_factor + dad_branch->lh_scale_factor;
    size_t nstates = aln->num_states;
    size_t ncat = site_rate->getNRate();

    size_t block = ncat * nstates;
    size_t ptn; // for big data size > 4GB memory required
    size_t c, i;
//    size_t orig_nptn = aln->size();
    size_t nptn = aln->size()+model_factory->unobserved_ptns.size();
//    double *eval = model->getEigenvalues();
//    assert(eval);


//	double prob_const = 0.0;
	memset(_pattern_lh_cat, 0, nptn*ncat*sizeof(double));

//    if (dad->isLeaf()) {
//    	// special treatment for TIP-INTERNAL NODE case
//    	double *partial_lh_node = new double[(aln->STATE_UNKNOWN+1)*block];
//    	IntVector states_dad = aln->seq_states[dad->id];
//    	states_dad.push_back(aln->STATE_UNKNOWN);
//    	// precompute information from one tip
//    	for (IntVector::iterator it = states_dad.begin(); it != states_dad.end(); it++) {
//    		double *lh_node = partial_lh_node +(*it)*block;
//    		double *lh_tip = tip_partial_lh + (*it)*nstates;
//    		double *val_tmp = val;
//			for (c = 0; c < ncat; c++) {
//				for (i = 0; i < nstates; i++) {
//					  lh_node[i] = val_tmp[i] * lh_tip[i];
//				}
//				lh_node += nstates;
//				val_tmp += nstates;
//			}
//    	}
//
//    	// now do the real computation
//#ifdef _OPENMP
//#pragma omp parallel for reduction(+: tree_lh, prob_const) private(ptn, i, c) schedule(static)
//#endif
//    	for (ptn = 0; ptn < nptn; ptn++) {
//			double lh_ptn = ptn_invar[ptn];
//			double *lh_cat = _pattern_lh_cat + ptn*ncat;
//			double *partial_lh_dad = dad_branch->partial_lh + ptn*block;
//			int state_dad = (ptn < orig_nptn) ? (aln->at(ptn))[dad->id] : model_factory->unobserved_ptns[ptn-orig_nptn];
//			double *lh_node = partial_lh_node + state_dad*block;
//			for (c = 0; c < ncat; c++) {
//				for (i = 0; i < nstates; i++) {
//					*lh_cat += lh_node[i] * partial_lh_dad[i];
//				}
//				lh_node += nstates;
//				partial_lh_dad += nstates;
//				lh_ptn += *lh_cat;
//				lh_cat++;
//			}
////			assert(lh_ptn > -1e-10);
//			if (ptn < orig_nptn) {
//				lh_ptn = log(fabs(lh_ptn));
//				_pattern_lh[ptn] = lh_ptn;
//				tree_lh += lh_ptn * ptn_freq[ptn];
//			} else {
//				prob_const += lh_ptn;
//			}
//		}
//		delete [] partial_lh_node;
//    } else {
    {
    	// both dad and node are internal nodes
#ifdef _OPENMP
#pragma omp parallel for reduction(+: tree_lh, prob_const) private(ptn, i, c) schedule(static)
#endif
    	for (ptn = 0; ptn < nptn; ptn++) {
			double lh_ptn = ptn_invar[ptn];
			double *lh_cat = _pattern_lh_cat + ptn*ncat;
			double *partial_lh_dad = dad_branch->partial_lh + ptn*block;
			double *partial_lh_node = node_branch->partial_lh + ptn*block;
//            double val[block];
//            for (c = 0; c < ncat; c++) {
//                double len = site_rate->getRate(c)*dad_branch->length;
//                double prop = site_rate->getProp(c);
//                for (i = 0; i < nstates; i++)
//                    val[c*nstates+i] = exp(eval[i]*len) * prop;
//            }
//			double *val_tmp = val;

            ModelSet *models = (ModelSet*)model;
            double *eval = models->at(ptn)->getEigenvalues();

			for (c = 0; c < ncat; c++) {
                double len = site_rate->getRate(c)*dad_branch->length;
                double prop = site_rate->getProp(c);
				for (i = 0; i < nstates; i++) {
					*lh_cat +=  exp(eval[i]*len) * prop * partial_lh_node[i] * partial_lh_dad[i];
				}
				lh_ptn += *lh_cat;
				partial_lh_node += nstates;
				partial_lh_dad += nstates;
//				val_tmp += nstates;
				lh_cat++;
			}

//			assert(lh_ptn > 0.0);
//            if (ptn < orig_nptn) 
            {
				lh_ptn = log(fabs(lh_ptn));
				_pattern_lh[ptn] = lh_ptn;
				tree_lh += lh_ptn * ptn_freq[ptn];
			}
//             else {
//				prob_const += lh_ptn;
//			}
		}
    }

    if (isnan(tree_lh) || isinf(tree_lh)) {
        cout << "WARNING: Numerical underflow caused by alignment sites";
        i = aln->getNSite();
        int j;
        for (j = 0, c = 0; j < i; j++) {
            ptn = aln->getPatternID(j);
            if (isnan(_pattern_lh[ptn]) || isinf(_pattern_lh[ptn])) {
                cout << " " << j+1;
                c++;
                if (c >= 10) {
                    cout << " ...";
                    break;
                }
            }
        }
        cout << endl;
        tree_lh = current_it->lh_scale_factor + current_it_back->lh_scale_factor;
        for (ptn = 0; ptn < nptn; ptn++) {
            if (isnan(_pattern_lh[ptn]) || isinf(_pattern_lh[ptn])) {
                _pattern_lh[ptn] = LOG_SCALING_THRESHOLD*4; // log(2^(-1024))
            }
            tree_lh += _pattern_lh[ptn] * ptn_freq[ptn];
        }
    }

//    if (orig_nptn < nptn) {
//    	// ascertainment bias correction
//        assert(prob_const < 1.0 && prob_const >= 0.0);
//
//        // BQM 2015-10-11: fix this those functions using _pattern_lh_cat
////        double inv_const = 1.0 / (1.0-prob_const);
////        size_t nptn_cat = orig_nptn*ncat;
////    	for (ptn = 0; ptn < nptn_cat; ptn++)
////            _pattern_lh_cat[ptn] *= inv_const;
//        
//    	prob_const = log(1.0 - prob_const);
//    	for (ptn = 0; ptn < orig_nptn; ptn++)
//    		_pattern_lh[ptn] -= prob_const;
//    	tree_lh -= aln->getNSite()*prob_const;
//		assert(!isnan(tree_lh) && !isinf(tree_lh));
//    }

	assert(!isnan(tree_lh) && !isinf(tree_lh));

//    delete [] val;
    return tree_lh;
}
