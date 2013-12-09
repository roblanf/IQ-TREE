/***************************************************************************
 *   Copyright (C) 2009 by BUI Quang Minh   *
 *   minh.bui@univie.ac.at   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <iqtree_config.h>
#include "phylotree.h"
#include "phylosupertree.h"
#include "phylosupertreeplen.h"
#include "phyloanalysis.h"
#include "alignment.h"
#include "superalignment.h"
#include "iqtree.h"
#include "gtrmodel.h"
#include "modeldna.h"
#include "myreader.h"
#include "rateheterogeneity.h"
#include "rategamma.h"
#include "rateinvar.h"
#include "rategammainvar.h"
//#include "modeltest_wrapper.h"
#include "modelprotein.h"
#include "modelbin.h"
#include "modelcodon.h"
#include "stoprule.h"

#include "mtreeset.h"
#include "mexttree.h"
#include "ratemeyerhaeseler.h"
#include "whtest_wrapper.h"
#include "partitionmodel.h"
#include "guidedbootstrap.h"
#include "modelset.h"
#include "timeutil.h"

#include "pll/pll.h"
#include "nnisearch.h"

void reportReferences(ofstream &out, string &original_model) {
	out
			<< "Bui Quang Minh, Minh Anh Thi Nguyen, and Arndt von Haeseler (2013) Ultrafast"
			<< endl << "approximation for phylogenetic bootstrap. Mol. Biol. Evol., 30:1188-1195."
			<< endl
			/*
			<< endl << "Lam-Tung Nguyen, Heiko A. Schmidt, Bui Quang Minh, and Arndt von Haeseler (2012)"
			<< endl
			<< "IQ-TREE: Efficient algorithm for phylogenetic inference by maximum likelihood"
			<< endl << "and important quartet puzzling. In prep." << endl*/
			<< endl << "For the original IQPNNI algorithm please cite: " << endl
			<< endl
			<< "Le Sy Vinh and Arndt von Haeseler (2004) IQPNNI: moving fast through tree space"
			<< endl
			<< "and stopping in time. Mol. Biol. Evol., 21:1565-1571."
			<< endl << endl;
	/*		"*** If you use the parallel version, please cite: " << endl << endl <<
	 "Bui Quang Minh, Le Sy Vinh, Arndt von Haeseler, and Heiko A. Schmidt (2005)" << endl <<
	 "pIQPNNI - parallel reconstruction of large maximum likelihood phylogenies." << endl <<
	 "Bioinformatics, 21:3794-3796." << endl << endl;*/

// 	if (original_model == "TEST" || original_model == "TESTONLY")
// 		out << "Since you used Modeltest please also cite Posada and Crandall (1998)" << endl << endl;
}

void reportAlignment(ofstream &out, Alignment &alignment) {
	out << "Input data: " << alignment.getNSeq() << " sequences with "
			<< alignment.getNSite() << " "
			<< ((alignment.num_states == 2) ?
					"binary" :
					((alignment.num_states == 4) ? "nucleotide" : (alignment.num_states == 20) ? "amino-acid" : "codon"))
			<< " sites" << endl << "Number of constant sites: "
			<< round(alignment.frac_const_sites * alignment.getNSite())
			<< " (= " << alignment.frac_const_sites * 100 << "% of all sites)"
			<< endl << "Number of site patterns: " << alignment.size() << endl
			<< endl;
}

void pruneModelInfo(vector<ModelInfo> &model_info, PhyloSuperTree *tree) {
	vector<ModelInfo> res_info;
	for (vector<PartitionInfo>::iterator it = tree->part_info.begin(); it != tree->part_info.end(); it++) {
		for (vector<ModelInfo>::iterator mit = model_info.begin(); mit != model_info.end(); mit++)
			if (mit->set_name == it->name)
				res_info.push_back(*mit);
	}
	model_info = res_info;

}

void reportModelSelection(ofstream &out, Params &params, vector<ModelInfo> &model_info, bool is_partitioned) {
	out << "Best-fit model according to "
		<< ((params.model_test_criterion == MTC_BIC) ? "BIC" :
			((params.model_test_criterion == MTC_AIC) ? "AIC" : "AICc")) << ": ";
	vector<ModelInfo>::iterator it;
	if (is_partitioned) {
		string set_name = "";
		for (it = model_info.begin(); it != model_info.end(); it++) {
			if (it->set_name != set_name) {
				if (set_name != "")
					out << ",";
				out << it->name << ":" << it->set_name;
				set_name = it->set_name;
			}
		}
	} else {
		out << model_info[0].name;
	}

	out << endl << endl << "List of models sorted by "
		<< ((params.model_test_criterion == MTC_BIC) ? "BIC" :
			((params.model_test_criterion == MTC_AIC) ? "AIC" : "AICc"))
		<< " scores: " << endl << endl;
	if (is_partitioned)
		out << "  ID  ";
	out << "Model             LogL          AIC      w-AIC      AICc     w-AICc       BIC      w-BIC" << endl;
	if (is_partitioned)
		out << "----------";

	out << "----------------------------------------------------------------------------------------" << endl;
	int setid = 1;
	for (it = model_info.begin(); it != model_info.end(); it++) {
		if (it->AIC_score == DBL_MAX) continue;
		if (it != model_info.begin() && it->set_name != (it-1)->set_name)
			setid++;
		if (is_partitioned) {
			out.width(4);
			out << right << setid << "  ";
		}
		out.width(13);
		out << left << it->name << " ";
		out.width(11);
		out << right << it->logl << " ";
		out.width(11);
		out	<< it->AIC_score << ((it->AIC_conf) ? " + " : " - ") << it->AIC_weight << " ";
		out.width(11);
		out << it->AICc_score << ((it->AICc_conf) ? " + " : " - ") << it->AICc_weight << " ";
		out.width(11);
		out << it->BIC_score  << ((it->BIC_conf) ? " + " : " - ") << it->BIC_weight;
		out << endl;
	}
	out << endl;
	out <<  "AIC, w-AIC   : Akaike information criterion scores and weights." << endl
		 << "AICc, w-AICc : Corrected AIC scores and weights." << endl
		 << "BIC, w-BIC   : Bayesian information criterion scores and weights." << endl << endl

		 << "Plus signs denote the 95% confidence sets." << endl
		 << "Minus signs denote significant exclusion." <<endl;
	out << endl;
}

void reportModel(ofstream &out, PhyloTree &tree) {
	int i, j, k;
	out << "Model of substitution: " << tree.getModelName() << endl << endl;

	if (tree.aln->num_states <= 4) {
		out << "Rate parameter R:" << endl << endl;

		double *rate_mat = new double[tree.aln->num_states * tree.aln->num_states];
		if (!tree.getModel()->isSiteSpecificModel())
			tree.getModel()->getRateMatrix(rate_mat);
		else
			((ModelSet*) tree.getModel())->front()->getRateMatrix(rate_mat);
		if (tree.aln->num_states > 4)
			out << fixed;
		if (tree.getModel()->isReversible()) {
			for (i = 0, k = 0; i < tree.aln->num_states - 1; i++)
				for (j = i + 1; j < tree.aln->num_states; j++, k++) {
					out << "  " << tree.aln->convertStateBackStr(i) << "-"
							<< tree.aln->convertStateBackStr(j) << ": " << rate_mat[k];
					if (tree.aln->num_states <= 4)
						out << endl;
					else if (k % 5 == 4)
						out << endl;
				}
		} else { // non-reversible model
			for (i = 0, k = 0; i < tree.aln->num_states; i++)
				for (j = 0; j < tree.aln->num_states; j++)
					if (i != j) {
						out << "  " << tree.aln->convertStateBackStr(i) << "-"
								<< tree.aln->convertStateBackStr(j) << ": "
								<< rate_mat[k];
						if (tree.aln->num_states <= 4)
							out << endl;
						else if (k % 5 == 4)
							out << endl;
						k++;
					}

		}

		//if (tree.aln->num_states > 4)
			out << endl;
		out.unsetf(ios_base::fixed);
		delete[] rate_mat;
	}
	out << "State frequencies: ";
	if (tree.getModel()->isSiteSpecificModel())
		out << "(site specific frequencies)" << endl << endl;
	else {
		if (!tree.getModel()->isReversible())
			out << "(inferred from Q matrix)" << endl;
		else
			switch (tree.getModel()->getFreqType()) {
			case FREQ_EMPIRICAL:
				out << "(empirical counts from alignment)" << endl;
				break;
			case FREQ_ESTIMATE:
				out << "(estimated with maximum likelihood)" << endl;
				break;
			case FREQ_USER_DEFINED:
				out << "(user-defined)" << endl;
				break;
			case FREQ_EQUAL:
				out << "(equal frequencies)" << endl;
				break;
			default:
				break;
			}
		out << endl;

		double *state_freqs = new double[tree.aln->num_states];
		tree.getModel()->getStateFrequency(state_freqs);
		for (i = 0; i < tree.aln->num_states; i++)
			out << "  pi(" << tree.aln->convertStateBackStr(i) << ") = "
					<< state_freqs[i] << endl;
		delete[] state_freqs;
		out << endl;
		if (tree.aln->num_states <= 4) {
			// report Q matrix
			double *q_mat = new double[tree.aln->num_states * tree.aln->num_states];
			tree.getModel()->getQMatrix(q_mat);

			out << "Rate matrix Q:" << endl << endl;
			for (i = 0, k = 0; i < tree.aln->num_states; i++) {
				out << "  " << tree.aln->convertStateBackStr(i);
				for (j = 0; j < tree.aln->num_states; j++, k++) {
					out << "  ";
					out.width(8);
					out << q_mat[k];
				}
				out << endl;
			}
			out << endl;
			delete[] q_mat;
		}
	}
}

void reportRate(ofstream &out, PhyloTree &tree) {
	int i;
	RateHeterogeneity *rate_model = tree.getRate();
	out << "Model of rate heterogeneity: " << rate_model->full_name << endl;
	rate_model->writeInfo(out);

	if (rate_model->getNDiscreteRate() > 1 || rate_model->getPInvar() > 0.0) {
		out << endl << " Category  Relative_rate  Proportion" << endl;
		if (rate_model->getPInvar() > 0.0)
			out << "  0         0              " << rate_model->getPInvar()
					<< endl;
		int cats = rate_model->getNDiscreteRate();
		DoubleVector prop;
		if (rate_model->getGammaShape() > 0 || rate_model->getPtnCat(0) < 0)
			prop.resize(cats,
					(1.0 - rate_model->getPInvar()) / rate_model->getNRate());
		else {
			prop.resize(cats, 0.0);
			for (i = 0; i < tree.aln->getNPattern(); i++)
				prop[rate_model->getPtnCat(i)] += tree.aln->at(i).frequency;
			for (i = 0; i < cats; i++)
				prop[i] /= tree.aln->getNSite();
		}
		for (i = 0; i < cats; i++) {
			out << "  " << i + 1 << "         ";
			out.width(14);
			out << left << rate_model->getRate(i) << " " << prop[i];
			out << endl;
		}
		if (rate_model->getGammaShape() > 0) {
			out << "Relative rates are computed as " << ((dynamic_cast<RateGamma*>(rate_model)->isCutMedian()) ? "median" : "mean") <<
				" of the portion of the Gamma distribution falling in the category." << endl;
		}
	}
	/*
	 if (rate_model->getNDiscreteRate() > 1 || rate_model->isSiteSpecificRate())
	 out << endl << "See file " << rate_file << " for site-specific rates and categories" << endl;*/
	out << endl;
}

void reportTree(ofstream &out, Params &params, PhyloTree &tree, double tree_lh,
		double lh_variance) {
	double epsilon = 1.0 / tree.getAlnNSite();
	double totalLen = tree.treeLength();
	out << "Total tree length (sum of branch lengths): " << totalLen << endl;
	double totalLenInternal = tree.treeLengthInternal(epsilon);
	out << "Sum of internal branch lengths: " << totalLenInternal << endl;
	out << "Sum of internal branch lengths divided by total tree length: "
			<< totalLenInternal / totalLen << endl;
	out << endl;
	//out << "ZERO BRANCH EPSILON = " << epsilon << endl;
	int zero_branches = tree.countZeroBranches(NULL, NULL, epsilon);
	if (zero_branches > 0) {
		int zero_internal_branches = tree.countZeroInternalBranches(NULL, NULL,
				epsilon);
		out << "WARNING: " << zero_branches
				<< " branches of near-zero lengths (<" << epsilon << ") and should be treated with caution!"
				<< endl;
		out << "WARNING: " << zero_internal_branches
				<< " internal branches of near-zero lengths (<" << epsilon << ") and should be treated with caution"
				<< endl;
		cout << endl << "WARNING: " << zero_branches
				<< " branches of near-zero lengths (<" << epsilon << ") and should be treated with caution!"
				<< endl;
		out << "         Such branches are denoted by '**' in the figure"
				<< endl << endl;
	}
	int long_branches = tree.countLongBranches(NULL, NULL, MAX_BRANCH_LEN-0.2);
	if (long_branches > 0) {
		stringstream sstr;
		sstr << "WARNING: " << long_branches
				<< " too long branches (>" << MAX_BRANCH_LEN-0.2 << ") should be treated with caution!"
				<< endl;
		out << sstr.str();
		cout << sstr.str();
	}
	tree.sortTaxa();
	//tree.setExtendedFigChar();
	tree.drawTree(out, WT_BR_SCALE + WT_INT_NODE, epsilon);
	int df = tree.getModelFactory()->getNParameters();
	int ssize = tree.getAlnNSite();
	double AIC_score, AICc_score, BIC_score;
	computeInformationScores(tree_lh, df, ssize, AIC_score, AICc_score, BIC_score);

	out << "Log-likelihood of the tree: " << fixed << tree_lh << " (s.e. "
			<< sqrt(lh_variance) << ")" << endl
			<< "Number of free parameters: " << df << endl
			<< "Akaike information criterion (AIC) score: " << AIC_score << endl
			<< "Corrected Akaike information criterion (AICc) score: " << AICc_score << endl
			<< "Bayesian information criterion (BIC) score: " << BIC_score << endl
			<< "Unconstrained log-likelihood (without tree): "
			<< tree.aln->computeUnconstrainedLogL() << endl << endl
			//<< "Total tree length: " << tree.treeLength() << endl << endl
			<< "Tree in newick format:" << endl << endl;

	tree.printTree(out, WT_BR_LEN | WT_BR_LEN_FIXED_WIDTH | WT_SORT_TAXA);

	out << endl << endl;
}

void reportCredits(ofstream &out) {
	out << "CREDITS" << endl << "-------" << endl << endl
			<< "Some parts of the code were taken from the following packages/libraries:"
			<< endl << endl
			<< "Schmidt HA, Strimmer K, Vingron M, and von Haeseler A (2002)" << endl
			<< "TREE-PUZZLE: maximum likelihood phylogenetic analysis using quartets" << endl
			<< "and parallel computing. Bioinformatics, 18(3):502-504." << endl << endl

			//<< "The source code to construct the BIONJ tree were taken from BIONJ software:"
			//<< endl << endl
			<< "Gascuel O (1997) BIONJ: an improved version of the NJ algorithm" << endl
			<< "based on a simple model of sequence data. Mol. Bio. Evol., 14:685-695." << endl << endl

			//<< "The Nexus file parser was taken from the Nexus Class Library:"
			//<< endl << endl
			<< "Paul O. Lewis (2003) NCL: a C++ class library for interpreting data files in" << endl
			<< "NEXUS format. Bioinformatics, 19(17):2330-2331." << endl << endl

			<< "Mascagni M and Srinivasan A (2000) Algorithm 806: SPRNG: A Scalable Library" << endl
			<< "for Pseudorandom Number Generation. ACM Transactions on Mathematical Software," << endl
			<< "26: 436-461." << endl << endl

			<< "Guennebaud G, Jacob B, et al. (2010) Eigen v3. http://eigen.tuxfamily.org" << endl << endl;
			/*
			<< "The Modeltest 3.7 source codes were taken from:" << endl << endl
			<< "David Posada and Keith A. Crandall (1998) MODELTEST: testing the model of"
			<< endl << "DNA substitution. Bioinformatics, 14(9):817-8." << endl
			*/
}

void reportPhyloAnalysis(Params &params, string &original_model,
		Alignment &alignment, IQTree &tree, vector<ModelInfo> &model_info) {
	string outfile = params.out_prefix;

	outfile += ".iqtree";
	try {
		ofstream out;
		out.exceptions(ios::failbit | ios::badbit);
		out.open(outfile.c_str());
		out << "IQ-TREE " << iqtree_VERSION_MAJOR << "." << iqtree_VERSION_MINOR
				<< "." << iqtree_VERSION_PATCH << " built " << __DATE__ << endl
				<< endl;
		if (params.partition_file)
			out << "Partition file name: " << params.partition_file << endl;
		if (params.aln_file)
			out << "Input file name: " << params.aln_file << endl;

		if (params.user_file)
			out << "User tree file name: " << params.user_file << endl;
		out << "Type of analysis: ";
		if (params.compute_ml_tree)
			out << "tree reconstruction";
		if (params.num_bootstrap_samples > 0) {
			if (params.compute_ml_tree)
				out << " + ";
			out << "non-parametric bootstrap (" << params.num_bootstrap_samples
					<< " replicates)";
		}
		out << endl;
		out << "Random seed number: " << params.ran_seed << endl << endl;
		out << "REFERENCES" << endl << "----------" << endl << endl;
		reportReferences(out, original_model);

		out << "SEQUENCE ALIGNMENT" << endl << "------------------" << endl
				<< endl;
		if (tree.isSuperTree()) {
			out << "Input data: " << alignment.getNSeq() << " taxa with "
					<< alignment.getNSite() << " partitions and "
					<< tree.getAlnNSite() << " total sites ("
					<< ((SuperAlignment*)tree.aln)->computeMissingData()*100 << "% missing data)" << endl << endl;

			PhyloSuperTree *stree = (PhyloSuperTree*) &tree;
			int namelen = stree->getMaxPartNameLength();
			int part;
			out.width(namelen+7);
			out << left << "  ID  Name" << " #Seqs  #Sites  #Patterns  #Const_Sites" << endl;
			out << string(namelen+46, '-') << endl;
			part = 0;
			for (PhyloSuperTree::iterator it = stree->begin(); it != stree->end(); it++, part++) {
				//out << "FOR PARTITION " << stree->part_info[part].name << ":" << endl << endl;
				//reportAlignment(out, *((*it)->aln));
				out.width(4);
				out << right << part+1 << "  ";
				out.width(namelen);
				out << left << stree->part_info[part].name << "  ";
				out.width(5);
				out << right << (*it)->aln->getNSeq() << "  ";
				out.width(6);
				out << (*it)->aln->getNSite() << "  ";
				out.width(6);
				out << (*it)->aln->getNPattern() << "      ";
				out << round((*it)->aln->frac_const_sites*100) << "%" << endl;
			}
			out << endl;
		} else
			reportAlignment(out, alignment);

		out.precision(4);
		out << fixed;

		if (!model_info.empty()) {
			out << "MODEL SELECTION" << endl << "---------------" << endl << endl;
			if (tree.isSuperTree())
				pruneModelInfo(model_info, (PhyloSuperTree*)&tree);
			reportModelSelection(out, params, model_info, tree.isSuperTree());
		}

		out << "SUBSTITUTION PROCESS" << endl << "--------------------" << endl
				<< endl;
		if (tree.isSuperTree()) {
			out	<< "Full partition model with separate branch lengths and models between partitions" << endl << endl;
			PhyloSuperTree *stree = (PhyloSuperTree*) &tree;
			PhyloSuperTree::iterator it;
			int part;

			out << "  ID  Model" << endl;
			out << "-----------------" << endl;
			for (it = stree->begin(), part = 0; it != stree->end(); it++, part++) {
				out.width(4);
				out << right << (part+1) << "  ";
				out << left << (*it)->getModelName() << endl;
			}
			out << endl;
			for (it = stree->begin(), part = 0; it != stree->end(); it++, part++) {
				out << "FOR PARTITION " << stree->part_info[part].name << ":" << endl << endl;
				reportModel(out, *(*it));
				reportRate(out, *(*it));
			}
		} else {
			reportModel(out, tree);
			reportRate(out, tree);
		}

		/*
		out << "RATE HETEROGENEITY" << endl << "------------------" << endl
				<< endl;
		if (tree.isSuperTree()) {
			PhyloSuperTree *stree = (PhyloSuperTree*) &tree;
			int part = 0;
			for (PhyloSuperTree::iterator it = stree->begin();
					it != stree->end(); it++, part++) {
				out << "FOR PARTITION " << stree->part_info[part].name << ":"
						<< endl << endl;
				reportRate(out, *(*it));
			}
		} else
			reportRate(out, tree);
		*/
		// Bootstrap analysis:
		//Display as outgroup: a

		if (original_model == "WHTEST") {
			out << "TEST OF MODEL HOMOGENEITY" << endl
					<< "-------------------------" << endl << endl;
			out << "Delta of input data:                 "
					<< params.whtest_delta << endl;
			out << ".95 quantile of Delta distribution:  "
					<< params.whtest_delta_quantile << endl;
			out << "Number of simulations performed:     "
					<< params.whtest_simulations << endl;
			out << "P-value:                             "
					<< params.whtest_p_value << endl;
			if (params.whtest_p_value < 0.05) {
				out
						<< "RESULT: Homogeneity assumption is rejected (p-value cutoff 0.05)"
						<< endl;
			} else {
				out
						<< "RESULT: Homogeneity assumption is NOT rejected (p-value cutoff 0.05)"
						<< endl;
			}
			out << endl << "*** For this result please cite:" << endl << endl;
			out
					<< "G. Weiss and A. von Haeseler (2003) Testing substitution models"
					<< endl
					<< "within a phylogenetic tree. Mol. Biol. Evol, 20(4):572-578"
					<< endl << endl;
		}

		out << "TREE SEARCH" << endl << "-----------" << endl << endl
				<< "Stopping rule: "
				<< ((params.stop_condition == SC_STOP_PREDICT) ? "Yes" : "No")
				<< endl << "Number of iterations: "
				<< tree.stop_rule.getNumIterations() << endl
				<< "Probability of deleting sequences: " << params.p_delete
				<< endl << "Number of representative leaves: "
				<< params.k_representative << endl
				<< "NNI log-likelihood cutoff: " << tree.getNNICutoff() << endl
				<< endl;

		if (params.compute_ml_tree) {
			out << "MAXIMUM LIKELIHOOD TREE" << endl
					<< "-----------------------" << endl << endl;

			tree.setRootNode(params.root);
			out << "NOTE: Tree is UNROOTED although outgroup taxon '"
					<< tree.root->name << "' is drawn at root" << endl;
			if (params.partition_file)
				out
						<< "NOTE: Branch lengths are weighted average over all partitions"
						<< endl
						<< "      (weighted by the number of sites in the partitions)"
						<< endl;
			if (params.aLRT_replicates > 0 || params.gbo_replicates
					|| (params.num_bootstrap_samples && params.compute_ml_tree)) {
				out << "Numbers in parentheses are ";
				if (params.aLRT_replicates > 0)
					out << "SH-aLRT supports";
				if (params.num_bootstrap_samples && params.compute_ml_tree)
					out << " standard bootstrap supports";
				if (params.gbo_replicates)
					out << " ultrafast bootstrap supports";
				out << " (%)" << endl;
			}
			out << endl;
			reportTree(out, params, tree, tree.getBestScore(),
					tree.logl_variance);

			if (tree.isSuperTree()) {
				PhyloSuperTree *stree = (PhyloSuperTree*) &tree;
				int empty_branches = stree->countEmptyBranches();
				if (empty_branches) {
					stringstream ss;
					ss << empty_branches << " undefined branch lengths in the overall tree!";
					outWarning(ss.str());
				}
				int part = 0;
				for (PhyloSuperTree::iterator it = stree->begin();
						it != stree->end(); it++, part++) {
					out << "FOR PARTITION " << stree->part_info[part].name
							<< ":" << endl << endl;
					string root_name;
					if (params.root)
						root_name = params.root;
					else
						root_name = (*it)->aln->getSeqName(0);
					(*it)->root = (*it)->findNodeName(root_name);
					assert((*it)->root);
					reportTree(out, params, *(*it), (*it)->computeLikelihood(),
							(*it)->computeLogLVariance());
				}
			}

		}
		/*
		 if (params.write_intermediate_trees) {
		 out << endl << "CONSENSUS OF INTERMEDIATE TREES" << endl << "-----------------------" << endl << endl
		 << "Number of intermediate trees: " << tree.stop_rule.getNumIterations() << endl
		 << "Split threshold: " << params.split_threshold << endl
		 << "Burn-in: " << params.tree_burnin << endl << endl;
		 }*/

		if (params.consensus_type == CT_CONSENSUS_TREE) {
			out << "CONSENSUS TREE" << endl << "--------------" << endl << endl;
			out << "Consensus tree is constructed from "
					<< (params.num_bootstrap_samples ?
							params.num_bootstrap_samples : params.gbo_replicates)
					<< " bootstrap trees" << endl
					<< "Branches with bootstrap support >"
					<< floor(params.split_threshold * 1000) / 10
					<< "% are kept";
			if (params.split_threshold == 0.0)
				out << " (extended consensus)";
			if (params.split_threshold == 0.5)
				out << " (majority-rule consensus)";
			if (params.split_threshold >= 0.99)
				out << " (strict consensus)";

			out << endl
					<< "Branch lengths are optimized by maximum likelihood on original alignment"
					<< endl;
			out << "Numbers in parentheses are bootstrap supports (%)" << endl
					<< endl;

			string con_file = params.out_prefix;
			con_file += ".contree";
			bool rooted = false;

			tree.freeNode();
			tree.readTree(con_file.c_str(), rooted);
			tree.setAlignment(tree.aln);
			tree.initializeAllPartialLh();
			tree.fixNegativeBranch(false);
			if (tree.isSuperTree())
				((PhyloSuperTree*) &tree)->mapTrees();
			tree.optimizeAllBranches();
			tree.printTree(con_file.c_str(),
					WT_BR_LEN | WT_BR_LEN_FIXED_WIDTH | WT_SORT_TAXA);
			tree.sortTaxa();
			tree.drawTree(out);
			out << endl << "Consensus tree in newick format: " << endl << endl;
			tree.printResultTree(out);
			out << endl << endl;
		}

		/* evaluate user trees */
		vector<TreeInfo> info;
		IntVector distinct_trees;
		if (params.treeset_file) {
			evaluateTrees(params, &tree, info, distinct_trees);
			out.precision(4);

			out << endl << "USER TREES" << endl << "----------" << endl << endl;
			out << "See " << params.treeset_file << ".trees for trees with branch lengths." << endl << endl;
			if (params.topotest_replicates && info.size() > 1) {
				if (params.do_weighted_test) {
					out << "Tree      logL    deltaL  bp-RELL    p-KH     p-SH    p-WKH    p-WSH    c-ELW" << endl;
					out << "-------------------------------------------------------------------------------" << endl;
				} else {
					out << "Tree      logL    deltaL  bp-RELL    p-KH     p-SH    c-ELW" << endl;
					out << "-------------------------------------------------------------" << endl;

				}
			} else {
				out << "Tree      logL    deltaL" << endl;
				out << "-------------------------" << endl;

			}
			double maxL = -DBL_MAX;
			int tid, orig_id;
			for (tid = 0; tid < info.size(); tid++)
				if (info[tid].logl > maxL) maxL = info[tid].logl;
			for (orig_id = 0, tid = 0; orig_id < distinct_trees.size(); orig_id++) {
				out.width(3);
				out << right << orig_id+1 << " ";
				if (distinct_trees[orig_id] >= 0) {
					out << " = tree " << distinct_trees[orig_id]+1 << endl;
					continue;
				}
				out.precision(3);
				out.width(12);
				out << info[tid].logl << " ";
				out.width(7);
				out << maxL - info[tid].logl;
				if (!params.topotest_replicates || info.size() <= 1) {
					out << endl;
					tid++;
					continue;
				}
				out.precision(4);
				out << "  ";
				out.width(6);
				out << info[tid].rell_bp;
				if (info[tid].rell_confident)
					out << " + ";
				else
					out << " - ";
				out.width(6);
				out << right << info[tid].kh_pvalue;
				if (info[tid].kh_pvalue < 0.05)
					out << " - ";
				else
					out << " + ";
				out.width(6);
				out << right << info[tid].sh_pvalue;
				if (info[tid].sh_pvalue < 0.05)
					out << " - ";
				else
					out << " + ";
				if (params.do_weighted_test) {
					out.width(6);
					out << right << info[tid].wkh_pvalue;
					if (info[tid].wkh_pvalue < 0.05)
						out << " - ";
					else
						out << " + ";
					out.width(6);
					out << right << info[tid].wsh_pvalue;
					if (info[tid].wsh_pvalue < 0.05)
						out << " - ";
					else
						out << " + ";
				}
				out.width(6);
				out << info[tid].elw_value;
				if (info[tid].elw_confident)
					out << " +";
				else
					out << " -";
				out << endl;
				tid++;
			}
			out << endl;

			if (params.topotest_replicates) {
				out <<  "deltaL  : logL difference from the maximal logl in the set." << endl
					 << "bp-RELL : bootstrap proportion using RELL method (Kishino et al. 1990)." << endl
					 << "p-KH    : p-value of one sided Kishino-Hasegawa test (1989)." << endl
					 << "p-SH    : p-value of Shimodaira-Hasegawa test (2000)." << endl;
				if (params.do_weighted_test) {
					out << "p-WKH   : p-value of weighted KH test." << endl
					 << "p-WSH   : p-value of weighted SH test." << endl;
				}
				out	 << "c-ELW   : Expected Likelihood Weight (Strimmer & Rambaut 2002)." << endl << endl
					 << "Plus signs denote the 95% confidence sets." << endl
					 << "Minus signs denote significant exclusion."  << endl
					 << "All tests performed "
					 << params.topotest_replicates << " resamplings using the RELL method."<<endl;
			}
			out << endl;
		}


		time_t cur_time;
		time(&cur_time);

		char *date_str;
		date_str = ctime(&cur_time);
		out.unsetf(ios_base::fixed);
		out << "TIME STAMP" << endl << "----------" << endl << endl
				<< "Date and time: " << date_str << "Total CPU time used: "
				<< (double) params.run_time << " seconds (" << convert_time(params.run_time) << ")" << endl
				<< "Total wall-clock time used: " << getRealTime() - params.start_real_time
				<< " seconds (" << convert_time(getRealTime() - params.start_real_time) << ")" << endl << endl;

		//reportCredits(out); // not needed, now in the manual
		out.close();

	} catch (ios::failure) {
		outError(ERR_WRITE_OUTPUT, outfile);
	}

	cout << endl << "Analysis results written to: " << endl
			<< "  IQ-TREE report:           " << params.out_prefix << ".iqtree"
			<< endl;
	if (params.compute_ml_tree)
		cout << "  Maximum-likelihood tree:  " << params.out_prefix
				<< ".treefile" << endl;
	if (!params.user_file) {
		cout << "  BIONJ tree:               " << params.out_prefix << ".bionj"
				<< endl;
		if (params.bestStart) {
			cout << "  Parsimony tree:           " << params.out_prefix
					<< ".parsimony" << endl;
		}
	}
	if (!params.dist_file) {
		cout << "  Juke-Cantor distances:    " << params.out_prefix << ".jcdist"
				<< endl;
		if (params.compute_ml_dist)
			cout << "  Likelihood distances:     " << params.out_prefix
					<< ".mldist" << endl;
		if (params.partition_file)
			cout << "  Concatenated alignment:   " << params.out_prefix
					<< ".conaln" << endl;
	}
	if (tree.getRate()->getGammaShape() > 0)
		cout << "  Gamma-distributed rates:  " << params.out_prefix << ".rate"
				<< endl;

	if (tree.getRate()->isSiteSpecificRate()
			|| tree.getRate()->getPtnCat(0) >= 0)
		cout << "  Site-rates by MH model:   " << params.out_prefix << ".rate"
				<< endl;

	if (params.print_site_lh)
		cout << "  Site log-likelihoods:     " << params.out_prefix << ".sitelh"
				<< endl;

	if (params.write_intermediate_trees)
		cout << "  All intermediate trees:   " << params.out_prefix << ".treels"
				<< endl;

	if (params.gbo_replicates) {
		cout << endl << "Ultrafast bootstrap approximation results written to:"
				<< endl << "  Split support values:     " << params.out_prefix
				<< ".splits" << endl << "  Consensus tree:           "
				<< params.out_prefix << ".contree" << endl;
		if (params.print_ufboot_trees)
			cout << "  UFBoot trees:             " << params.out_prefix << ".ufboot" << endl;

	}

	if (params.treeset_file) {
		cout << "  Evaluated user trees:     " << params.out_prefix << ".trees" << endl;

		if (params.print_tree_lh) {
			cout << "  Tree log-likelihoods:   " << params.out_prefix << ".treelh" << endl;
		}
		if (params.print_site_lh) {
			cout << "  Site log-likelihoods:     " << params.out_prefix << ".sitelh" << endl;
		}
	}
	cout << "  Screen log file:          " << params.out_prefix << ".log"
			<< endl;
	/*	if (original_model == "WHTEST")
	 cout <<"  WH-TEST report:           " << params.out_prefix << ".whtest" << endl;*/
	cout << endl;

}

void checkZeroDist(Alignment *aln, double *dist) {
	int ntaxa = aln->getNSeq();
	IntVector checked;
	checked.resize(ntaxa, 0);
	int i, j;
	for (i = 0; i < ntaxa - 1; i++) {
		if (checked[i])
			continue;
		string str = "";
		bool first = true;
		for (j = i + 1; j < ntaxa; j++)
			if (dist[i * ntaxa + j] <= 1e-6) {
				if (first)
					str = "ZERO distance between sequences "
							+ aln->getSeqName(i);
				str += ", " + aln->getSeqName(j);
				checked[j] = 1;
				first = false;
			}
		checked[i] = 1;
		if (str != "")
			outWarning(str);
	}
}


void printAnalysisInfo(int model_df, IQTree& iqtree, Params& params) {
//	if (!params.raxmllib) {
	cout << "Model of evolution: ";
	if (iqtree.isSuperTree()) {
		cout << iqtree.getModelName() << " (" << model_df << " free parameters)" << endl;
	} else {
		cout << iqtree.getModelName() << " with ";
		switch (iqtree.getModel()->getFreqType()) {
		case FREQ_EQUAL:
			cout << "equal";
			break;
		case FREQ_EMPIRICAL:
			cout << "counted";
			break;
		case FREQ_USER_DEFINED:
			cout << "user-defined";
			break;
		case FREQ_ESTIMATE:
			cout << "optimized";
			break;
		case FREQ_CODON_1x4:
			cout << "counted 1x4";
			break;
		case FREQ_CODON_3x4:
			cout << "counted 3x4";
			break;
		case FREQ_CODON_3x4C:
			cout << "counted 3x4-corrected";
			break;
		default:
			outError("Wrong specified state frequencies");
		}
		cout << " frequencies (" << model_df << " free parameters)" << endl;
	}
	cout << "Fixed branch lengths: "
			<< ((params.fixed_branch_length) ? "Yes" : "No") << endl;
	cout << "Lambda for local search: " << params.lambda << endl;
	if (params.speed_conf != 1.0) {
		cout << "Confidence value for speed up NNI: ";
		if (params.new_heuristic)
			cout << "Using 50%*" << params.speed_conf << endl;
		else
			cout << "N" << params.speed_conf << " * delta" << params.speed_conf
					<< endl;
	} else {
		cout << "Speed up NNI: disabled " << endl;
	}
	cout << "NNI cutoff: " << params.nni_cutoff << endl;
	cout << "Approximate NNI: " << (params.approximate_nni ? "Yes" : "No")
			<< endl;
	cout << "Phylogenetic likelihood library: " << (params.pll ? "Yes" : "No") << endl;
	cout << endl;
}

void computeMLDist(double &longest_dist, string &dist_file, double begin_time,
		IQTree& iqtree, Params& params, Alignment* alignment, double &bestTreeScore) {
	stringstream best_tree_string;
	iqtree.printTree(best_tree_string, WT_BR_LEN + WT_TAXON_ID);
	cout << "Computing ML distances based on estimated model parameters...";
	double *ml_dist = NULL;
    double *ml_var = NULL;
    longest_dist = iqtree.computeDist(params, alignment, ml_dist, ml_var, dist_file);
	cout << " " << (getCPUTime() - begin_time) << " sec" << endl;
	if (longest_dist > MAX_GENETIC_DIST * 0.99) {
		outWarning("Some pairwise ML distances are too long (saturated)");
		//cout << "Some ML distances are too long, using old distances..." << endl;
	} //else
	{
		if ( !iqtree.dist_matrix ) {
	        iqtree.dist_matrix = new double[alignment->getNSeq() * alignment->getNSeq()];
		}
		if ( !iqtree.var_matrix ) {
	        iqtree.var_matrix = new double[alignment->getNSeq() * alignment->getNSeq()];
		}
		memmove(iqtree.dist_matrix, ml_dist,
                sizeof (double) * alignment->getNSeq() * alignment->getNSeq());
        memmove(iqtree.var_matrix, ml_var,
				sizeof(double) * alignment->getNSeq() * alignment->getNSeq());
	}
	delete[] ml_dist;
    delete[] ml_var;
}

void runPhyloAnalysis(Params &params, string &original_model,
		Alignment* &alignment, IQTree &iqtree, vector<ModelInfo> &model_info) {
	double t_begin, t_end;
	t_begin = getCPUTime();

	/* Initialized all data strucutre for PLL*/
	/* Set the PLL instance attributes */
	iqtree.pllAttr.rateHetModel = PLL_GAMMA;
	iqtree.pllAttr.fastScaling = PLL_FALSE;
	iqtree.pllAttr.saveMemory = PLL_FALSE;
	iqtree.pllAttr.useRecom = PLL_FALSE;
	iqtree.pllAttr.randomNumberSeed = params.ran_seed;
	iqtree.pllAttr.numberOfThreads = 8; /* This only affects the pthreads version */

	if ( iqtree.pllInst != NULL ) {
		pllDestroyInstance(iqtree.pllInst);
	}
	/* Create a PLL instance */
	iqtree.pllInst = pllCreateInstance(&iqtree.pllAttr);

	/* Read in the alignment file */
	string pllAln = params.out_prefix;
	pllAln += ".pllaln";
	alignment->printPhylip(pllAln.c_str());
	iqtree.pllAlignment = pllParseAlignmentFile(PLL_FORMAT_PHYLIP, pllAln.c_str());

	/* Read in the partition information */
	pllQueue *partitionInfo;
	if (params.partition_file) {
		partitionInfo = pllPartitionParse(params.partition_file);
	} else {
		/* create a partition file */
		string model;
		if (alignment->num_states == 4) {
			model = "DNA";
		} else if (alignment->num_states == 20) {
			model = "WAG"; // TODO: Change this hard-coded model
		}
		ofstream partFile;
		string filename = string(params.out_prefix) + ".partitions";
		partFile.open(filename.c_str());
		partFile << model << ", p1 = " << "1-" << alignment->getNSite() << endl;
		partFile.close();
		partitionInfo = pllPartitionParse(filename.c_str());
	}

	/* Validate the partitions */
	if (!pllPartitionsValidate(partitionInfo, iqtree.pllAlignment)) {
		fprintf(stderr, "Error: Partitions do not cover all sites\n");
		exit(EXIT_FAILURE);
	}

	/* Commit the partitions and build a partitions structure */
	iqtree.pllPartitions = pllPartitionsCommit(partitionInfo, iqtree.pllAlignment);

	/* We don't need the the intermedia partition queue structure anymore */
	pllQueuePartitionsDestroy(&partitionInfo);

	/* eliminate duplicate sites from the alignment and update weights vector */
	pllAlignmentRemoveDups(iqtree.pllAlignment, iqtree.pllPartitions);

	pllTreeInitTopologyForAlignment(iqtree.pllInst, iqtree.pllAlignment);

	/* Connect the alignment and partition structure with the tree structure */
	if (!pllLoadAlignment(iqtree.pllInst, iqtree.pllAlignment,
			iqtree.pllPartitions, PLL_SHALLOW_COPY)) {
		outError("Incompatible tree/alignment combination");
	}

	/*
	 cout << "Computing parsimony score..." << endl;
	 for (int i = 0; i < trees_block->GetNumTrees(); i++) {
	 stringstream strs(trees_block->GetTranslatedTreeDescription(i), ios::in | ios::out | ios::app);
	 strs << ";";
	 PhyloTree tree;
	 bool myrooted = trees_block->IsRootedTree(i);
	 tree.readTree(strs, myrooted);
	 tree.setAlignment(alignment);
	 int score = tree.computeParsimonyScore();
	 cout << "Tree " << trees_block->GetTreeName(i) << " has parsimony score of " << score << endl;
	 }
	 */

	/* initialize tree, either by user tree or BioNJ tree */
	double longest_dist;
	string dist_file;
	double begin_time = getCPUTime();
	params.startTime = begin_time;
	params.start_real_time = getRealTime();
    string bionj_file = params.out_prefix;
    bionj_file += ".bionj";

    /* TODO Discuss with Minh: Since I will completely replace the BIONJ tree with parsimony, there will
	 * be no need to compute JC distance per default anymore. JC will be introduced as an option. */

	// Compute JC distances or read them from user file
	if (params.dist_file) {
		cout << "Reading distance matrix file " << params.dist_file << " ..." << endl;
	} else if (params.compute_jc_dist && !params.bestStart) {
		cout << "Computing Juke-Cantor distances..." << endl;
	} else if (params.compute_obs_dist) {
		cout << "Computing observed distances..." << endl;
	}

	if (params.compute_jc_dist || params.compute_obs_dist) {
		longest_dist = iqtree.computeDist(params, alignment, iqtree.dist_matrix,
				iqtree.var_matrix, dist_file);
		checkZeroDist(alignment, iqtree.dist_matrix);
		if (longest_dist > MAX_GENETIC_DIST * 0.99) {
			outWarning("Some pairwise distances are too long (saturated)");
			//cout << "Some distances are too long, computing observed distances..." << endl;
			//longest_dist = iqtree.computeObsDist(params, alignment, iqtree.dist_matrix, dist_file);
			//assert(longest_dist <= 1.0);
		}
	}

	// start the search with user-defined tree
	if (params.user_file) {
		cout << "Reading input tree file " << params.user_file << " ..." << endl;
		bool myrooted = params.is_rooted;
		iqtree.readTree(params.user_file, myrooted);
		iqtree.setAlignment(alignment);
		// Create parsimony tree using IQ-Tree kernel
    } else if (params.parsimony_tree && !params.pll) {
		cout << endl;
		cout << "CREATING PARSIMONY TREE BY IQTree ..." << endl;
		iqtree.computeParsimonyTree(params.out_prefix, alignment);
    } else if (params.bestStart) {
    	cout << "Creating an initial parsimony tree ... ";
    	double start = getCPUTime();
		// generate a parsimony tree for model optimization
		iqtree.pllInst->randomNumberSeed = params.ran_seed;
		pllComputeRandomizedStepwiseAdditionParsimonyTree(iqtree.pllInst,
				iqtree.pllPartitions);
		Tree2String(iqtree.pllInst->tree_string, iqtree.pllInst,
				iqtree.pllPartitions, iqtree.pllInst->start->back, PLL_FALSE,
				PLL_TRUE, PLL_FALSE, PLL_FALSE, PLL_FALSE, PLL_SUMMARIZE_LH,
				PLL_FALSE, PLL_FALSE);
		/* input the first tree into IQTree to optimize model parameters */
		stringstream parsTreeString;
		parsTreeString << iqtree.pllInst->tree_string;
		iqtree.readTree(parsTreeString, iqtree.rooted);
		iqtree.setAlignment(alignment);
		double end = getCPUTime();
		cout << end - start << " seconds" << endl;
	} else {
		cout << "Creating an initial BIONJ tree ... ";
		double start = getCPUTime();
		// This is the old default option: using BIONJ as starting tree
		iqtree.computeBioNJ(params, alignment, dist_file);
		cout << getCPUTime() - start << " seconds" << endl;
	}

	if (params.root) {
		string str = params.root;
		if (!iqtree.findNodeName(str)) {
			str = "Specified root name " + str + "not found";
			outError(str);
		}
	}

    /* Fix if negative branch lengths detected */
    //double fixed_length = 0.001;
    int fixed_number = iqtree.fixNegativeBranch(false);
    if (fixed_number) {
        cout << "WARNING: " << fixed_number << " undefined/negative branch lengths are initialized with parsimony" << endl;
    }

	bool test_only = params.model_name.substr(0,8) == "TESTONLY";
	/* initialize substitution model */
	if (params.model_name.substr(0,4) == "TEST") {
		if (iqtree.isSuperTree())
			((PhyloSuperTree*) &iqtree)->mapTrees();
		uint64_t mem_size = iqtree.getMemoryRequired();
		mem_size *= (params.num_rate_cats+1);
	    cout << "NOTE: MODEL SELECTION REQUIRES AT LEAST " << ((double) mem_size * sizeof(double) / 1024.0) / 1024 << " MB MEMORY!" << endl;
	    if (mem_size >= getMemorySize()) {
	    	outError("Memory required exceeds your computer RAM size!");
	    }
		params.model_name = testModel(params, &iqtree, model_info);
		cout << "CPU time for model selection: " << getCPUTime() - t_begin << " seconds." << endl;
		alignment = iqtree.aln;
		if (test_only) {
			params.min_iterations = 0;
		}
	}

	if (params.model_name == "WHTEST") {
		if (alignment->num_states != 4)
			outError("Weiss & von Haeseler test of model homogeneity only works for DNA");
		params.model_name = "GTR+G";
	}

	assert(iqtree.aln);
	iqtree.optimize_by_newton = params.optimize_by_newton;
	iqtree.sse = params.SSE;
	if (params.gbo_replicates)
		params.speed_conf = 1.0;
	if (params.speed_conf == 1.0)
		iqtree.disableHeuristic();
	else
		iqtree.setSpeed_conf(params.speed_conf);
	try {
		if (!iqtree.getModelFactory()) {
			if (iqtree.isSuperTree()){
				if(params.partition_type){
					iqtree.setModelFactory(new PartitionModelPlen(params, (PhyloSuperTreePlen*) &iqtree));
				} else
					iqtree.setModelFactory(new PartitionModel(params, (PhyloSuperTree*) &iqtree));
			} else {
				iqtree.setModelFactory(new ModelFactory(params, &iqtree));
			}
		}
	} catch (string & str) {
		outError(str);
	}
	iqtree.setModel(iqtree.getModelFactory()->model);
	iqtree.setRate(iqtree.getModelFactory()->site_rate);
	iqtree.setStartLambda(params.lambda);
	if (iqtree.isSuperTree())
			((PhyloSuperTree*) &iqtree)->mapTrees();

    // string to store the current tree with taxon id and branch lengths
    stringstream initTree;
    iqtree.setParams(params);

	// degree of freedom
	int model_df = iqtree.getModelFactory()->getNParameters();
	cout << endl;
	cout << "ML-TREE SEARCH START WITH THE FOLLOWING PARAMETERS:" << endl;
	printAnalysisInfo(model_df, iqtree, params);

	uint64_t mem_size = iqtree.getMemoryRequired();
    cout << "NOTE: THE ANALYSIS REQUIRES AT LEAST " << ((double) mem_size * sizeof(double) / 1024.0) / 1024 << " MB MEMORY!" << endl;
    if (mem_size >= getMemorySize()) {
    	outError("Memory required exceeds your computer RAM size!");
    }

    cout.precision(6);
	cout << "Optimize model parameters " << (params.optimize_model_rate_joint ? "jointly":"")
			<< " (tolerace " << params.model_eps << ")... " << endl;

    // Optimize model parameters and branch lengths using ML for the initial tree
    iqtree.curScore = iqtree.getModelFactory()->optimizeParameters(params.fixed_branch_length, true, params.model_eps);
    iqtree.bestScore = iqtree.curScore;

	// Save current tree to a string
	iqtree.printTree(initTree);

	// Compute maximum likelihood distance
	double bestTreeScore = iqtree.bestScore;
	if (params.inni || params.min_iterations == 1) {
		params.compute_ml_dist = false;
	}
	if (!params.dist_file && params.compute_ml_dist) {
		computeMLDist(longest_dist, dist_file, getCPUTime(), iqtree, params, alignment, bestTreeScore);
	}

    if (!params.fixed_branch_length && params.leastSquareBranch) {
        cout << "Computing Least Square branch lengths " << endl;
        iqtree.optimizeAllBranchesLS();
        iqtree.curScore = iqtree.computeLikelihood();
        iqtree.printResultTree("LeastSquareTree");
	}

	double t_tree_search_start, t_tree_search_end;
	t_tree_search_start = getCPUTime();

	if (params.parsimony) {
		iqtree.enable_parsimony = true;
		iqtree.pars_scores = new double[3000];
		iqtree.lh_scores = new double[3000];
		for (int i = 0; i < 3000; i++) {
			iqtree.pars_scores[i] = 0;
			iqtree.lh_scores[i] = 0;
		}
		iqtree.cur_pars_score = iqtree.computeParsimony();
	}

	int numParsTree;
	if (params.user_file) {
		numParsTree = 1;
	}else if (params.random_restart) {
		numParsTree = iqtree.stop_rule.getNumIterations();
		iqtree.stop_rule.setIterationNum(1,1);
	} else if (params.user_file){
		numParsTree = 1;
	} else {
		numParsTree = params.numParsimony;
	}

	if (params.min_iterations > 0) {
		double initTime = getCPUTime();
		string *parsTree = new string[numParsTree];
		parsTree[0] = initTree.str();
		int nni_count, nni_steps;
		nni_count = 0;
		nni_steps = 0;
		string bestTreeString = initTree.str();
		pllNewickTree *newick = pllNewickParseString(parsTree[0].c_str());
		pllTreeInitTopologyNewick(iqtree.pllInst, newick, PLL_FALSE);
		pllInitModel(iqtree.pllInst, iqtree.pllPartitions, iqtree.pllAlignment);
		iqtree.inputModelParam2PLL();
		pllTreeInitTopologyNewick(iqtree.pllInst, newick, PLL_FALSE);
		pllNewickParseDestroy(&newick);
		iqtree.pllUpdateBestTree();

		for (int treeNr = 0; treeNr < numParsTree; treeNr++) {
			if (treeNr >= 1) {
				iqtree.pllInst->randomNumberSeed = params.ran_seed + treeNr * 12345;
				pllComputeRandomizedStepwiseAdditionParsimonyTree(iqtree.pllInst, iqtree.pllPartitions);
				Tree2String(iqtree.pllInst->tree_string, iqtree.pllInst,
						iqtree.pllPartitions, iqtree.pllInst->start->back,
						PLL_FALSE,
						PLL_TRUE, PLL_FALSE, PLL_FALSE, PLL_FALSE,
						PLL_SUMMARIZE_LH,
						PLL_FALSE, PLL_FALSE);
				string parsTreeString(iqtree.pllInst->tree_string);
				iqtree.readTreeString(parsTreeString);
				stringstream tree;
				iqtree.printTree(tree);
				parsTree[treeNr] = tree.str();
			}
			if (params.pll) {
				pllNewickTree *newick = pllNewickParseString(parsTree[treeNr].c_str());
				pllTreeInitTopologyNewick(iqtree.pllInst, newick, PLL_FALSE);
				pllEvaluateGeneric(iqtree.pllInst, iqtree.pllPartitions, iqtree.pllInst->start, PLL_TRUE, PLL_FALSE);
				pllTreeEvaluate(iqtree.pllInst, iqtree.pllPartitions, 8);
				pllNewickParseDestroy(&newick);
				iqtree.curScore = iqtree.pllInst->likelihood;
				cout << "logl of starting tree " << treeNr + 1 << ": " << iqtree.curScore << endl;
				SearchInfo searchinfo;
				iqtree.curScore = iqtree.pllOptimizeNNI(nni_count, nni_steps, searchinfo);
				cout << "logl of fastNNI " << treeNr + 1 << ": "
						<< iqtree.curScore << " (NNIs: " << nni_count
						<< " / NNI steps: " << nni_steps << ")" << endl;
				if (iqtree.curScore > iqtree.bestScore) {
					iqtree.bestScore = iqtree.curScore;
					cout << "BETTER SCORE FOUND: " << iqtree.bestScore << endl;
					Tree2String(iqtree.pllInst->tree_string, iqtree.pllInst,
							iqtree.pllPartitions, iqtree.pllInst->start->back,
							PLL_TRUE, PLL_TRUE, PLL_FALSE, PLL_FALSE, PLL_FALSE,
							PLL_SUMMARIZE_LH, PLL_FALSE, PLL_FALSE);
					bestTreeString = string(iqtree.pllInst->tree_string);
					iqtree.pllUpdateBestTree();
				}
			} else {
				stringstream parsTreeString;
				parsTreeString << parsTree[treeNr];
				iqtree.readTree(parsTreeString, iqtree.rooted);
				iqtree.setAlignment(alignment);
				iqtree.initializeAllPartialLh();
				iqtree.curScore = iqtree.optimizeAllBranches();
				cout << "logl of starting tree " << treeNr + 1 << ": "
						<< iqtree.curScore << endl;
				iqtree.curScore = iqtree.optimizeNNI(nni_count, nni_steps);
				cout << "logl of fastNNI " << treeNr + 1 << ": "
						<< iqtree.curScore << " (NNIs: " << nni_count
						<< " / NNI steps: " << nni_steps << ")" << endl;
				if (iqtree.curScore > iqtree.bestScore) {
					iqtree.bestScore = iqtree.curScore;
					stringstream tree;
					iqtree.printTree(tree);
					bestTreeString = tree.str();
				}
			}
			double min_elapsed = (getCPUTime() - begin_time) / 60;
			if (min_elapsed > params.maxtime) {
				cout << "Maximal running time of " << params.maxtime << " minutes reached" << endl;
				break;
			}
		}
		cout << "Best loglh = " << iqtree.bestScore << " / CPU time: " << getCPUTime() - initTime << endl;
		if (params.pll) {
			/* Read in the best tree in PLL */
			if (!restoreTree(iqtree.pllBestTree, iqtree.pllInst, iqtree.pllPartitions)) {
				outError("ERROR: failed to roll back tree \n");
			}
		}

		/* IQTree kernel: read in the best tree */
		iqtree.readTreeString(bestTreeString);
		// re-estimate model parameters for the best found local optimal tree
	    iqtree.curScore = iqtree.getModelFactory()->optimizeParameters(params.fixed_branch_length, true, params.model_eps);
	    initTree.seekp(0, ios::beg);
	    iqtree.printTree(initTree);
		newick = pllNewickParseString(initTree.str().c_str());
		pllTreeInitTopologyNewick(iqtree.pllInst, newick, PLL_FALSE);
		iqtree.inputModelParam2PLL();
		pllNewickParseDestroy(&newick);
		pllEvaluateGeneric(iqtree.pllInst, iqtree.pllPartitions, iqtree.pllInst->start, PLL_TRUE, PLL_FALSE);
	    cout << "Log-likelihood recomputed by PLL: " << iqtree.pllInst->likelihood << endl;
	    if (abs(iqtree.pllInst->likelihood - iqtree.curScore) > 0.1) {
	    	cout << "POSSIBLE ERROR: logl computed by PLL deviates too much from IQTree kernel" << endl;
	    }
		iqtree.curScore = iqtree.pllInst->likelihood;
	    iqtree.bestScore = iqtree.curScore;

		// deallocate partial likelihood within IQTree kernel to save memory when PLL is used */
		if (params.pll)
			iqtree.deleteAllPartialLh();

		delete[] parsTree;

		/* FOR PARTITION MODEL */
		if (iqtree.isSuperTree())
			((PhyloSuperTree*) &iqtree)->computeBranchLengths();
	}


	if (iqtree.isSuperTree())
		((PhyloSuperTree*) &iqtree)->computeBranchLengths();

	/*
	 if ((tree.getModel()->name == "JC") && tree.getRate()->getNDim() == 0)
	 params.compute_ml_dist = false;*/

	/* do NNI with likelihood function */
	//bool saved_estimate_nni = estimate_nni_cutoff;
	//estimate_nni_cutoff = false; // do not estimate NNI cutoff based on initial BIONJ tree

    if (params.leastSquareNNI) {
    	iqtree.computeSubtreeDists();
    }
    iqtree.setRootNode(params.root); // Important for NNI below

	//estimate_nni_cutoff = saved_estimate_nni;
	if (original_model == "WHTEST") {
		cout << endl
				<< "Testing model homogeneity by Weiss & von Haeseler (2003)..."
				<< endl;
		WHTest(params, iqtree);
	}

	/*double sum_scaling = 1.0;
	 if (!tree.checkEqualScalingFactor(sum_scaling))
	 cout << "Scaling factor not equal along the tree" << endl;*/

	NodeVector pruned_taxa;
	StrVector linked_name;
	double *saved_dist_mat = iqtree.dist_matrix;
	double *pattern_lh;
	int num_low_support;
	double mytime;

	pattern_lh = new double[iqtree.getAlnNPattern()];

	if (params.aLRT_threshold <= 100
			&& (params.aLRT_replicates > 0 || params.localbp_replicates > 0)) {
		mytime = getCPUTime();
		cout << "Testing tree branches by SH-like aLRT with "
				<< params.aLRT_replicates << " replicates..." << endl;
		iqtree.setRootNode(params.root);
		iqtree.computePatternLikelihood(pattern_lh, &iqtree.curScore);
		num_low_support = iqtree.testAllBranches(params.aLRT_threshold,
				iqtree.curScore, pattern_lh, params.aLRT_replicates,
				params.localbp_replicates);
		iqtree.printResultTree();
		cout << "  " << getCPUTime() - mytime << " sec." << endl;
		cout << num_low_support << " branches show low support values (<= "
				<< params.aLRT_threshold << "%)" << endl;

		//tree.drawTree(cout);
		cout << "Collapsing stable clades..." << endl;
		iqtree.collapseStableClade(params.aLRT_threshold, pruned_taxa,
				linked_name, iqtree.dist_matrix);
		cout << pruned_taxa.size() << " taxa were pruned from stable clades"
				<< endl;
	}

	if (!pruned_taxa.empty()) {
		cout << "Pruned alignment contains " << iqtree.aln->getNSeq()
				<< " sequences and " << iqtree.aln->getNSite() << " sites and "
				<< iqtree.aln->getNPattern() << " patterns" << endl;
		//tree.clearAllPartialLh();
		iqtree.initializeAllPartialLh();
		iqtree.clearAllPartialLH();
		iqtree.curScore = iqtree.optimizeAllBranches();
		//cout << "Log-likelihood	after reoptimizing model parameters: " << tree.curScore << endl;
		int nni_count, nni_steps;
		iqtree.curScore = iqtree.optimizeNNI(nni_count, nni_steps);
		cout << "Log-likelihood after optimizing partial tree: "
				<< iqtree.curScore << endl;
		/*
		 pattern_lh = new double[tree.getAlnSize()];
		 double score = tree.computeLikelihood(pattern_lh);
		 num_low_support = tree.testAllBranches(params.aLRT_threshold, score, pattern_lh, params.aLRT_replicates);
		 tree.drawTree(cout);
		 delete [] pattern_lh;*/
	}
	// set p delete if ZERO
	/*
	 if (params.p_delete == 0) {
	 int num_high_support = tree.leafNum - 3 - num_low_support;
	 params.p_delete = (2.0 + num_high_support)*2.0 / tree.leafNum;
	 if (params.p_delete > 0.5) params.p_delete = 0.5;
	 }*/

	//tree.setParams(params);
	/* evaluating all trees in user tree file */

	/* DO IQPNNI */
	if (params.k_representative > 0 /*&&  params.min_iterations > 1*/) {
		if (params.inni) {
			cout << endl << "START ITERATED NNI SEARCH WITH THE FOLLOWING PARAMETERS" << endl;
		} else {
			cout << endl << "START IQPNNI SEARCH WITH THE FOLLOWING PARAMETERS" << endl;
		}
		if (!params.inni) {
			cout << "Number of representative leaves  : " << params.k_representative << endl;
			cout << "Probability of deleting sequences: " << iqtree.getProbDelete() << endl;
			cout << "Number of leaves to be deleted   : " << iqtree.getDelete() << endl;
		} else if (params.inni) {
			cout << "Probability of weak perturbation: " << params.prob_weak << endl;
			cout << "Weak perturbation: " << params.perturb_weak << endl;
			cout << "Strong perturbation: " << params.perturb_strong << endl;
		}
		cout << "Number of iterations: ";
		if (params.stop_condition == SC_FIXED_ITERATION)
			cout << params.min_iterations << endl;
		else
			cout << "predicted in [" << params.min_iterations << ","
					<< params.max_iterations << "] (confidence "
					<< params.stop_confidence << ")" << endl;
		if (!params.inni) {
			cout << "Important quartets assessed on: "
					<< ((params.iqp_assess_quartet == IQP_DISTANCE) ?
							"Distance" : ((params.iqp_assess_quartet == IQP_PARSIMONY) ? "Parsimony" : "Bootstrap"))
					<< endl;
		}

		cout << "NNI assessed on: " << ((params.nni5) ? "5 branches" : "1 branch") << endl;
		cout << "SSE instructions: "
				<< ((iqtree.sse) ? "Yes" : "No") << endl;
		cout << "Branch length optimization method   : "
				<< ((iqtree.optimize_by_newton) ? "Newton" : "Brent") << endl;
		cout << "Phylogenetic likelihood library(PLL): " << (params.pll ? "Yes" : "No") << endl;
		cout << endl;
		//saveTree( iqtree.pllInst, iqtree.pllBestTree, iqtree.pllPartitions->numberOfPartitions );
		iqtree.doIQPNNI();
		iqtree.setAlignment(alignment);
	} else {
		/* do SPR with likelihood function */
		if (params.tree_spr) {
			//tree.optimizeSPRBranches();
			cout << "Doing SPR Search" << endl;
			cout << "Start tree.optimizeSPR()" << endl;
			double spr_score = iqtree.optimizeSPR();
			cout << "Finish tree.optimizeSPR()" << endl;
			//double spr_score = tree.optimizeSPR(tree.curScore, (PhyloNode*) tree.root->neighbors[0]->node);
			if (spr_score <= iqtree.curScore) {
				cout << "SPR search did not found any better tree" << endl;
			}
		}

	}

	if (!pruned_taxa.empty()) {
		iqtree.disableHeuristic();
		cout << "Restoring full tree..." << endl;
		iqtree.restoreStableClade(alignment, pruned_taxa, linked_name);
		delete[] iqtree.dist_matrix;
		iqtree.dist_matrix = saved_dist_mat;
		iqtree.initializeAllPartialLh();
		iqtree.clearAllPartialLH();
		iqtree.curScore = iqtree.optimizeAllBranches();
		//cout << "Log-likelihood	after reoptimizing model parameters: " << tree.curScore << endl;
		int nni_count, nni_steps;
		iqtree.curScore = iqtree.optimizeNNI(nni_count, nni_steps);
		cout << "Log-likelihood	after reoptimizing full tree: "
				<< iqtree.curScore << endl;		//iqtree.setBestScore(iqtree.getModelFactory()->optimizeParameters(params.fixed_branch_length, true, params.model_eps));

	}

	if (iqtree.isSuperTree())
			((PhyloSuperTree*) &iqtree)->mapTrees();

	if (params.min_iterations) {
		cout << endl;
		iqtree.setAlignment(alignment);
		iqtree.initializeAllPartialLh();
		iqtree.clearAllPartialLH();
		cout << "Optimizing model parameters" << endl;
		iqtree.setBestScore(iqtree.getModelFactory()->optimizeParameters(params.fixed_branch_length, true, TOL_LIKELIHOOD_PARAMOPT));
	} else {
		iqtree.setBestScore(iqtree.curScore);
	}

	if (iqtree.isSuperTree())
		((PhyloSuperTree*) &iqtree)->computeBranchLengths();

	cout << endl;
	cout << "BEST SCORE FOUND : " << iqtree.getBestScore() << endl;
	t_tree_search_end = getCPUTime();
	double treeSearchTime = (t_tree_search_end - t_tree_search_start);

	/* root the tree at the first sequence */
	iqtree.root = iqtree.findLeafName(alignment->getSeqName(0));
	assert(iqtree.root);

	double myscore = 0.0;

	myscore = iqtree.getBestScore();
	iqtree.computePatternLikelihood(pattern_lh, &myscore);

	// compute logl variance
	iqtree.logl_variance = iqtree.computeLogLVariance();


	if (params.print_site_lh) {
		string site_lh_file = params.out_prefix;
		site_lh_file += ".sitelh";
		if (params.print_site_lh == 1)
			printSiteLh(site_lh_file.c_str(), &iqtree, pattern_lh);
		else
			printSiteLhCategory(site_lh_file.c_str(), &iqtree);
	}

	if (params.mvh_site_rate) {
		RateMeyerHaeseler *rate_mvh = new RateMeyerHaeseler(params.rate_file,
				&iqtree, params.rate_mh_type);
		cout << endl << "Computing site-specific rates by "
				<< rate_mvh->full_name << "..." << endl;
		rate_mvh->runIterativeProc(params, iqtree);
		cout << endl << "BEST SCORE FOUND : " << iqtree.getBestScore() << endl;
		string mhrate_file = params.out_prefix;
		mhrate_file += ".mhrate";
		iqtree.getRate()->writeSiteRates(mhrate_file.c_str());

		if (params.print_site_lh) {
			string site_lh_file = params.out_prefix;
			site_lh_file += ".mhsitelh";
			printSiteLh(site_lh_file.c_str(), &iqtree);
		}
	}

	if ((params.aLRT_replicates > 0 || params.localbp_replicates > 0) && !iqtree.isSuperTree()) {
		mytime = getCPUTime();
		cout << endl;
		cout << "Testing tree branches by SH-like aLRT with "
				<< params.aLRT_replicates << " replicates..." << endl;
		iqtree.setRootNode(params.root);
		//if (tree.isSuperTree()) ((PhyloSuperTree*)&tree)->mapTrees();
		num_low_support = iqtree.testAllBranches(params.aLRT_threshold, myscore,
				pattern_lh, params.aLRT_replicates, params.localbp_replicates);
		//cout << num_low_support << " branches show low support values (<= " << params.aLRT_threshold << "%)" << endl;
		cout << "CPU Time used:  " << getCPUTime() - mytime << " sec." << endl;
		//delete [] pattern_lh;
		/*
		 string out_file = params.out_prefix;
		 out_file += ".alrt";
		 tree.writeInternalNodeNames(out_file);

		 cout << "Support values written to " << out_file << endl;*/
	}

	string rate_file = params.out_prefix;
	rate_file += ".rate";
	iqtree.getRate()->writeSiteRates(rate_file.c_str());

	if (iqtree.isSuperTree()) {
		PhyloSuperTree *stree = (PhyloSuperTree*) &iqtree;
		int part = 0;
		try {
			ofstream out;
			out.exceptions(ios::failbit | ios::badbit);
			out.open(rate_file.c_str());
			for (PhyloSuperTree::iterator it = stree->begin(); it != stree->end();
					it++, part++) {
				out << "SITE RATES FOR PARTITION " << stree->part_info[part].name << ":" << endl;
				(*it)->getRate()->writeSiteRates(out);
			}
			cout << "Site rates printed to " << rate_file << endl;
			out.close();
		} catch (ios::failure) {
			outError(ERR_WRITE_OUTPUT, rate_file);
		}
	}

	if (params.gbo_replicates > 0) {
		if (!params.online_bootstrap)
			runGuidedBootstrap(params, alignment, iqtree);
		else
			iqtree.summarizeBootstrap(params);
	}

	cout << "Total tree length: " << iqtree.treeLength() << endl;

	t_end = getCPUTime();
	params.run_time = (t_end - t_begin);
	cout << endl;
	cout << "CPU time used for tree reconstruction: " << treeSearchTime
			<< " sec (" << convert_time(treeSearchTime) << ")" << endl;
	cout << "Total CPU time used: " << (double) params.run_time << " sec ("
			<< convert_time((double) params.run_time) << ")" << endl;
	cout << "Total wall-clock time used: "
			<< getRealTime() - params.start_real_time << " sec ("
			<< convert_time(getRealTime() - params.start_real_time) << ")"
			<< endl;
	//printf( "Total time used: %8.6f seconds.\n", (double) params.run_time );

	iqtree.printResultTree();
    if (verbose_mode >= VB_MED && params.pll) {
        iqtree.printPhylolibTree(".phylolibtree_end");
    }
	if (params.out_file)
		iqtree.printTree(params.out_file);
	//tree.printTree(params.out_file,WT_BR_LEN_FIXED_WIDTH);

	else {
		//tree.printTree(cout);
		//cout << endl;
		/*
		 if (verbose_mode > VB_MED) {
		 if (verbose_mode >= VB_DEBUG)
		 tree.drawTree(cout, WT_BR_SCALE + WT_INT_NODE + WT_BR_LEN);
		 else
		 tree.drawTree(cout);
		 }*/
	}

	delete[] pattern_lh;

	/*	if (tree.getRate()->isSiteSpecificRate() || tree.getRate()->getPtnCat(0) >= 0) {
	 string rate_file = params.out_prefix;
	 rate_file += ".mhrate";
	 tree.getRate()->writeSiteRates(rate_file.c_str());
	 }*/

/*	if (verbose_mode >= VB_DEBUG)
		iqtree.printTransMatrices();
	*/
}

void runPhyloAnalysis(Params &params) {
	Alignment *alignment;
	IQTree *tree;
	vector<ModelInfo> model_info;
	// read in alignment
	if (params.partition_file) {
		if(params.partition_type){
			// initialize supertree - Proportional Edges case, "-spt p" option
			tree = new PhyloSuperTreePlen(params);
		} else {
			// initialize supertree stuff if user specifies partition file with -sp option
			tree = new PhyloSuperTree(params);
		}
		// this alignment will actually be of type SuperAlignment
		alignment = tree->aln;
	} else {
		alignment = new Alignment(params.aln_file, params.sequence_type, params.intype);
		tree = new IQTree(alignment);
	}
	string original_model = params.model_name;
	if (params.concatenate_aln) {
		Alignment aln(params.concatenate_aln, params.sequence_type, params.intype);
		cout << "Concatenating " << params.aln_file << " with " << params.concatenate_aln << " ..." << endl;
		alignment->concatenateAlignment(&aln);
	}

	if (params.aln_output) {
		// convert alignment to other format and write to output file
		if (params.num_bootstrap_samples || params.print_bootaln) {
			// create bootstrap alignment
			Alignment* bootstrap_alignment;
			cout << "Creating bootstrap alignment..." << endl;
			if (alignment->isSuperAlignment())
				bootstrap_alignment = new SuperAlignment;
			else
				bootstrap_alignment = new Alignment;
			bootstrap_alignment->createBootstrapAlignment(alignment, NULL, params.bootstrap_spec);
			delete alignment;
			alignment = bootstrap_alignment;
		}
		if (alignment->isSuperAlignment()) {
			((SuperAlignment*)alignment)->printCombinedAlignment(params.aln_output);
			if (params.print_subaln)
				((SuperAlignment*)alignment)->printSubAlignments(params, ((PhyloSuperTree*)tree)->part_info);

		} else if (params.gap_masked_aln) {
			Alignment out_aln;
			Alignment masked_aln(params.gap_masked_aln, params.sequence_type,
					params.intype);
			out_aln.createGapMaskedAlignment(&masked_aln, alignment);
			out_aln.printPhylip(params.aln_output, false, params.aln_site_list,
					params.aln_nogaps, params.ref_seq_name);
			string str = params.gap_masked_aln;
			str += ".sitegaps";
			out_aln.printSiteGaps(str.c_str());
		} else if (params.aln_output_format == ALN_PHYLIP)
			alignment->printPhylip(params.aln_output, false,
					params.aln_site_list, params.aln_nogaps,
					params.ref_seq_name);
		else if (params.aln_output_format == ALN_FASTA)
			alignment->printFasta(params.aln_output, false,
					params.aln_site_list, params.aln_nogaps,
					params.ref_seq_name);
	} else if (params.gbo_replicates > 0 && params.user_file
			&& params.second_tree) {
		// run one of the UFBoot analysis
		runGuidedBootstrap(params, alignment, *tree);
	} else if (params.avh_test) {
		// run one of the wondering test for Arndt
		runAvHTest(params, alignment, *tree);
	} else if (params.num_bootstrap_samples == 0) {
		// the main Maximum likelihood tree reconstruction
		alignment->checkGappySeq();
		runPhyloAnalysis(params, original_model, alignment, *tree, model_info);
		if (params.gbo_replicates && params.online_bootstrap) {

			cout << endl << "Computing consensus tree..." << endl;
			string splitsfile = params.out_prefix;
			splitsfile += ".splits.nex";
			//cout << splitsfile << endl;
			computeConsensusTree(splitsfile.c_str(), 0, 1e6, -1,
					params.split_threshold, NULL, params.out_prefix, NULL,
					&params);
		}
		//if (original_model != "TESTONLY")
			reportPhyloAnalysis(params, original_model, *alignment, *tree, model_info);
	} else {
		// the classical non-parameter bootstrap (SBS)
		// turn off aLRT test
		int saved_aLRT_replicates = params.aLRT_replicates;
		params.aLRT_replicates = 0;
		string treefile_name = params.out_prefix;
		treefile_name += ".treefile";
		string boottrees_name = params.out_prefix;
		boottrees_name += ".boottrees";
		string bootaln_name = params.out_prefix;
		bootaln_name += ".bootaln";
		string bootlh_name = params.out_prefix;
		bootlh_name += ".bootlh";
		// first empty the boottrees file
		try {
			ofstream tree_out;
			tree_out.exceptions(ios::failbit | ios::badbit);
			tree_out.open(boottrees_name.c_str());
			tree_out.close();
		} catch (ios::failure) {
			outError(ERR_WRITE_OUTPUT, boottrees_name);
		}

		// empty the bootaln file
		if (params.print_bootaln)
		try {
			ofstream tree_out;
			tree_out.exceptions(ios::failbit | ios::badbit);
			tree_out.open(bootaln_name.c_str());
			tree_out.close();
		} catch (ios::failure) {
			outError(ERR_WRITE_OUTPUT, bootaln_name);
		}

		double start_time = getCPUTime();

		// do bootstrap analysis
		for (int sample = 0; sample < params.num_bootstrap_samples; sample++) {
			cout << endl << "===> START BOOTSTRAP REPLICATE NUMBER "
					<< sample + 1 << endl << endl;

			Alignment* bootstrap_alignment;
			cout << "Creating bootstrap alignment..." << endl;
			if (alignment->isSuperAlignment())
				bootstrap_alignment = new SuperAlignment;
			else
				bootstrap_alignment = new Alignment;
			bootstrap_alignment->createBootstrapAlignment(alignment, NULL, params.bootstrap_spec);
			if (params.print_tree_lh) {
				double prob;
				bootstrap_alignment->multinomialProb(*alignment, prob);
				ofstream boot_lh;
				if (sample == 0)
					boot_lh.open(bootlh_name.c_str());
				else
					boot_lh.open(bootlh_name.c_str(),
							ios_base::out | ios_base::app);
				boot_lh << "0\t" << prob << endl;
				boot_lh.close();
			}
			IQTree *boot_tree;
			if (alignment->isSuperAlignment()){
				if(params.partition_type){
					boot_tree = new PhyloSuperTreePlen(
											(SuperAlignment*) bootstrap_alignment,
											(PhyloSuperTree*) tree);
				} else {
					boot_tree = new PhyloSuperTree(
											(SuperAlignment*) bootstrap_alignment,
											(PhyloSuperTree*) tree);
				}
			} else
				boot_tree = new IQTree(bootstrap_alignment);
			if (params.print_bootaln)
				bootstrap_alignment->printPhylip(bootaln_name.c_str(), true);
			runPhyloAnalysis(params, original_model, bootstrap_alignment,
					*boot_tree, model_info);
			// read in the output tree file
			string tree_str;
			try {
				ifstream tree_in;
				tree_in.exceptions(ios::failbit | ios::badbit);
				tree_in.open(treefile_name.c_str());
				tree_in >> tree_str;
				tree_in.close();
			} catch (ios::failure) {
				outError(ERR_READ_INPUT, treefile_name);
			}
			// write the tree into .boottrees file
			try {
				ofstream tree_out;
				tree_out.exceptions(ios::failbit | ios::badbit);
				tree_out.open(boottrees_name.c_str(),
						ios_base::out | ios_base::app);
				tree_out << tree_str << endl;
				tree_out.close();
			} catch (ios::failure) {
				outError(ERR_WRITE_OUTPUT, boottrees_name);
			}
			if (params.num_bootstrap_samples == 1)
				reportPhyloAnalysis(params, original_model,
						*bootstrap_alignment, *boot_tree, model_info);
			delete bootstrap_alignment;
		}

		if (params.consensus_type == CT_CONSENSUS_TREE) {

			cout << endl << "===> COMPUTE CONSENSUS TREE FROM "
					<< params.num_bootstrap_samples << " BOOTSTRAP TREES"
					<< endl << endl;
			computeConsensusTree(boottrees_name.c_str(), 0, 1e6, -1,
					params.split_threshold, NULL, params.out_prefix, NULL,
					&params);
		}

		if (params.compute_ml_tree) {
			cout << endl << "===> START ANALYSIS ON THE ORIGINAL ALIGNMENT"
					<< endl << endl;
			params.aLRT_replicates = saved_aLRT_replicates;
			runPhyloAnalysis(params, original_model, alignment, *tree, model_info);

			cout << endl
					<< "===> ASSIGN BOOTSTRAP SUPPORTS TO THE TREE FROM ORIGINAL ALIGNMENT"
					<< endl << endl;
			MExtTree ext_tree;
			assignBootstrapSupport(boottrees_name.c_str(), 0, 1e6,
					treefile_name.c_str(), false, treefile_name.c_str(),
					params.out_prefix, ext_tree, NULL, &params);
			tree->copyTree(&ext_tree);
			reportPhyloAnalysis(params, original_model, *alignment, *tree, model_info);
		} else if (params.consensus_type == CT_CONSENSUS_TREE) {
			int mi = params.min_iterations;
			STOP_CONDITION sc = params.stop_condition;
			params.min_iterations = 0;
			params.stop_condition = SC_FIXED_ITERATION;
			runPhyloAnalysis(params, original_model, alignment, *tree, model_info);
			params.min_iterations = mi;
			params.stop_condition = sc;
			tree->setIQPIterations(params.stop_condition,
					params.stop_confidence, params.min_iterations,
					params.max_iterations);
			reportPhyloAnalysis(params, original_model, *alignment, *tree, model_info);
		} else
			cout << endl;

		cout << "Total CPU time for bootstrap: " << (getCPUTime() - start_time)
				<< " seconds." << endl << endl;
		cout << "Non-parametric bootstrap results written to:" << endl;
		if (params.print_bootaln)
			cout << "  Bootstrap alignments:     " << params.out_prefix
				<< ".bootaln" << endl;
		cout << "  Bootstrap trees:          "
				<< params.out_prefix << ".boottrees" << endl;
		if (params.consensus_type == CT_CONSENSUS_TREE)
			cout << "  Consensus tree:           " << params.out_prefix
					<< ".contree" << endl;
		cout << endl;
	}

	//if(params.partition_type)
	//	((PhyloSuperTreePlen*)tree)->printNNIcasesNUM();

	delete tree;
	delete alignment;
}

void assignBranchSupportNew(Params &params) {
	if (!params.user_file)
		outError("No trees file provided");
	if (!params.second_tree)
		outError("No target tree file provided");
	cout << "Reading tree " << params.second_tree << " ..." << endl;
	MTree tree(params.second_tree, params.is_rooted);
	cout << tree.leafNum << " taxa and " << tree.branchNum << " branches" << endl;
	tree.assignBranchSupport(params.user_file);
	string str = params.second_tree;
	str += ".suptree";
	tree.printTree(str.c_str());
	cout << "Tree with assigned branch supports written to " << str << endl;
	if (verbose_mode >= VB_DEBUG)
		tree.drawTree(cout);
}



/**
 * assign split occurence frequencies from a set of input trees onto a target tree
 * NOTE: input trees must have the same taxon set
 * @param input_trees file containing NEWICK tree strings
 * @param burnin number of beginning trees to discard
 * @param max_count max number of trees to read in
 * @param target_tree the target tree
 * @param rooted TRUE if trees are rooted, false for unrooted trees
 * @param output_file file name to write output tree with assigned support values
 * @param out_prefix prefix of output file
 * @param mytree (OUT) resulting tree with support values assigned from target_tree
 * @param tree_weight_file file containing INTEGER weights of input trees
 * @param params program parameters
 */
void assignBootstrapSupport(const char *input_trees, int burnin, int max_count,
		const char *target_tree, bool rooted, const char *output_tree,
		const char *out_prefix, MExtTree &mytree, const char* tree_weight_file,
		Params *params) {
	//bool rooted = false;
	// read the tree file
	cout << "Reading tree " << target_tree << " ..." << endl;
	mytree.init(target_tree, rooted);
	// reindex the taxa in the tree to aphabetical names
	NodeVector taxa;
	mytree.getTaxa(taxa);
	sort(taxa.begin(), taxa.end(), nodenamecmp);
	int i = 0;
	for (NodeVector::iterator it = taxa.begin(); it != taxa.end(); it++) {
		(*it)->id = i++;
	}

	/*
	 string filename = params.boot_trees;
	 filename += ".nolen";
	 boot_trees.printTrees(filename.c_str(), false);
	 return;
	 */
	SplitGraph sg;
	SplitIntMap hash_ss;
	// make the taxa name
	vector<string> taxname;
	taxname.resize(mytree.leafNum);
	mytree.getTaxaName(taxname);

	// read the bootstrap tree file
	double scale = 100.0;
	if (params->scaling_factor > 0)
		scale = params->scaling_factor;

	MTreeSet boot_trees;
	if (params && detectInputFile((char*) input_trees) == IN_NEXUS) {
		sg.init(*params);
		for (SplitGraph::iterator it = sg.begin(); it != sg.end(); it++)
			hash_ss.insertSplit((*it), (*it)->getWeight());
		StrVector sgtaxname;
		sg.getTaxaName(sgtaxname);
		i = 0;
		for (StrVector::iterator sit = sgtaxname.begin();
				sit != sgtaxname.end(); sit++, i++) {
			Node *leaf = mytree.findLeafName(*sit);
			if (!leaf)
				outError("Tree does not contain taxon ", *sit);
			leaf->id = i;
		}
		scale /= sg.maxWeight();
	} else {
		boot_trees.init(input_trees, rooted, burnin, max_count,
				tree_weight_file);
		boot_trees.convertSplits(taxname, sg, hash_ss, SW_COUNT, -1);
		scale /= boot_trees.sumTreeWeights();
	}
	//sg.report(cout);
	cout << "Rescaling split weights by " << scale << endl;
	if (params->scaling_factor < 0)
		sg.scaleWeight(scale, true);
	else {
		sg.scaleWeight(scale, false, params->numeric_precision);
	}

	cout << sg.size() << " splits found" << endl;
	// compute the percentage of appearance
	//	printSplitSet(sg, hash_ss);
	//sg.report(cout);
	cout << "Creating bootstrap support values..." << endl;
	mytree.createBootstrapSupport(taxname, boot_trees, sg, hash_ss);
	//mytree.scaleLength(100.0/boot_trees.size(), true);
	string out_file;
	if (output_tree)
		out_file = output_tree;
	else {
		if (out_prefix)
			out_file = out_prefix;
		else
			out_file = target_tree;
		out_file += ".suptree";
	}

	mytree.printTree(out_file.c_str());
	cout << "Tree with assigned bootstrap support written to " << out_file
			<< endl;
	/*
	if (out_prefix)
		out_file = out_prefix;
	else
		out_file = target_tree;
	out_file += ".supval";
	mytree.writeInternalNodeNames(out_file);

	cout << "Support values written to " << out_file << endl;
	*/
}

void computeConsensusTree(const char *input_trees, int burnin, int max_count,
		double cutoff, double weight_threshold, const char *output_tree,
		const char *out_prefix, const char *tree_weight_file, Params *params) {
	bool rooted = false;

	// read the bootstrap tree file
	/*
	 MTreeSet boot_trees(input_trees, rooted, burnin, tree_weight_file);
	 string first_taxname = boot_trees.front()->root->name;
	 //if (params.root) first_taxname = params.root;

	 SplitGraph sg;

	 boot_trees.convertSplits(sg, cutoff, SW_COUNT, weight_threshold);*/

	//sg.report(cout);
	SplitGraph sg;
	SplitIntMap hash_ss;
	// make the taxa name
	//vector<string> taxname;
	//taxname.resize(mytree.leafNum);
	//mytree.getTaxaName(taxname);

	// read the bootstrap tree file
	double scale = 100.0;
	if (params->scaling_factor > 0)
		scale = params->scaling_factor;

	MTreeSet boot_trees;
	if (params && detectInputFile((char*) input_trees) == IN_NEXUS) {
		char *user_file = params->user_file;
		params->user_file = (char*) input_trees;
		params->split_weight_summary = SW_COUNT; // count number of splits
		sg.init(*params);
		params->user_file = user_file;
		for (SplitGraph::iterator it = sg.begin(); it != sg.end(); it++)
			hash_ss.insertSplit((*it), (*it)->getWeight());
		/*		StrVector sgtaxname;
		 sg.getTaxaName(sgtaxname);
		 i = 0;
		 for (StrVector::iterator sit = sgtaxname.begin(); sit != sgtaxname.end(); sit++, i++) {
		 Node *leaf = mytree.findLeafName(*sit);
		 if (!leaf) outError("Tree does not contain taxon ", *sit);
		 leaf->id = i;
		 }*/
		scale /= sg.maxWeight();
	} else {
		boot_trees.init(input_trees, rooted, burnin, max_count,
				tree_weight_file);
		boot_trees.convertSplits(sg, cutoff, SW_COUNT, weight_threshold);
		scale /= boot_trees.sumTreeWeights();
		cout << sg.size() << " splits found" << endl;
	}
	//sg.report(cout);
	cout << "Rescaling split weights by " << scale << endl;
	if (params->scaling_factor < 0)
		sg.scaleWeight(scale, true);
	else {
		sg.scaleWeight(scale, false, params->numeric_precision);
	}



	cout << "Creating greedy consensus tree..." << endl;
	MTree mytree;
	SplitGraph maxsg;
	sg.findMaxCompatibleSplits(maxsg);

	if (verbose_mode >= VB_MAX)
		maxsg.saveFileStarDot(cout);
	cout << "convert compatible split system into tree..." << endl;
	mytree.convertToTree(maxsg);
	//cout << "done" << endl;
	string taxname = sg.getTaxa()->GetTaxonLabel(0);
	Node *node = mytree.findLeafName(taxname);
	if (node)
		mytree.root = node;
	// mytree.scaleLength(100.0 / boot_trees.sumTreeWeights(), true);

	// mytree.getTaxaID(maxsg.getSplitsBlock()->getCycle());
	//maxsg.saveFile(cout);

	string out_file;

	if (output_tree)
		out_file = output_tree;
	else {
		if (out_prefix)
			out_file = out_prefix;
		else
			out_file = input_trees;
		out_file += ".contree";
	}

	mytree.printTree(out_file.c_str(), WT_BR_CLADE);
	cout << "Consensus tree written to " << out_file << endl;

	if (output_tree)
		out_file = output_tree;
	else {
		if (out_prefix)
			out_file = out_prefix;
		else
			out_file = input_trees;
		out_file += ".splits";
	}

    //sg.scaleWeight(0.01, false, 4);
    sg.saveFile(out_file.c_str(), IN_OTHER, true);
    cout << "Non-trivial split supports printed to star-dot file " << out_file << endl;

}

void computeConsensusNetwork(const char *input_trees, int burnin, int max_count,
		double cutoff, int weight_summary, double weight_threshold, const char *output_tree,
		const char *out_prefix, const char* tree_weight_file) {
	bool rooted = false;

	// read the bootstrap tree file
	MTreeSet boot_trees(input_trees, rooted, burnin, max_count,
			tree_weight_file);

	SplitGraph sg;
	//SplitIntMap hash_ss;

	boot_trees.convertSplits(sg, cutoff, weight_summary, weight_threshold);

	string out_file;

	if (output_tree)
		out_file = output_tree;
	else {
		if (out_prefix)
			out_file = out_prefix;
		else
			out_file = input_trees;
		out_file += ".nex";
	}

	sg.saveFile(out_file.c_str(), IN_NEXUS);
	cout << "Consensus network printed to " << out_file << endl;

	if (output_tree)
		out_file = output_tree;
	else {
		if (out_prefix)
			out_file = out_prefix;
		else
			out_file = input_trees;
		out_file += ".splits";
	}

	sg.saveFile(out_file.c_str(), IN_OTHER, true);
    cout << "Non-trivial split supports printed to star-dot file " << out_file << endl;

}
