#include "estimateRFSpecificDesign.h"
#include <math.h>
#include "intercrossingAndSelfingGenerations.h"
#include "estimateRFCheckFunnels.h"
#include <map>
#include <set>
#include "recodeFoundersFinalsHets.h"
#include <memory>
#include "constructLookupTable.hpp"
#include "probabilities2.hpp"
#include "probabilities4.hpp"
#include "alleleDataErrors.h"
#include "recodeHetsAsNA.h"
#include "estimateRF.h"
#include "matrixChunks.h"
#ifdef USE_OPENMP
#include "mpMap2_openmp.h"
#include <omp.h>
#endif
template<int nFounders, int maxAlleles, bool infiniteSelfing> bool estimateRFSpecificDesign(rfhaps_internal_args& args, unsigned long long& progressCounter)
{
	std::size_t nFinals = args.finals.nrow(), nRecombLevels = args.recombinationFractions.size();
	std::size_t nDifferentFunnels = args.funnelEncodings.size();
	std::vector<double>& lineWeights = args.lineWeights;
	Rcpp::List finalDimNames = args.finals.attr("dimnames");
	Rcpp::CharacterVector finalNames = finalDimNames[0];

	int nMarkerPatternIDs = (int)args.markerPatternData.allMarkerPatterns.size();
	int maxAIGenerations = *std::max_element(args.intercrossingGenerations.begin(), args.intercrossingGenerations.end());
	int minSelfing = *std::min_element(args.selfingGenerations.begin(), args.selfingGenerations.end());
	int maxSelfing = *std::max_element(args.selfingGenerations.begin(), args.selfingGenerations.end());

	//This is basically just a huge lookup table
	allMarkerPairData<maxAlleles> computedContributions(nMarkerPatternIDs);
	
	constructLookupTableArgs<maxAlleles, nFounders> lookupArgs(computedContributions, args.markerPatternData);
	lookupArgs.recombinationFractions = &args.recombinationFractions;
	lookupArgs.funnelEncodings = &args.funnelEncodings;
	lookupArgs.intercrossingGenerations = &args.intercrossingGenerations;
	lookupArgs.selfingGenerations = &args.selfingGenerations;
	constructLookupTable<nFounders, maxAlleles, infiniteSelfing>(lookupArgs);

	//We parallelise this array, even though it's over an iterator not an integer. So we use an integer and use that to work out how many steps forwards we need to move the iterator. We assume that the values are strictly increasing, otherwise this will never work. 
	//Use this to only call setTxtProgressBar every 10 calls to updateProgress. Probably no point in updating status more frequently than that.
	unsigned long long updateProgressCounter = 0;
#ifdef USE_OPENMP
	#pragma omp parallel 
#endif
	{
		triangularIterator indexIterator = args.startPosition;
		int previousCounter = 0;
#ifdef USE_OPENMP
		#pragma omp for schedule(static, 1)
#endif
		for(int counter = 0; counter < args.valuesToEstimateInChunk; counter++)
		{
			int difference = counter - previousCounter;
			if(difference < 0) throw std::runtime_error("Internal error");
			while(difference > 0) 
			{
				indexIterator.next();
				difference--;
			}
			previousCounter = counter;

			std::pair<int, int> markerIndices = indexIterator.get();
			int markerCounterRow = markerIndices.first, markerCounterColumn = markerIndices.second;

			int markerPatternID1 = args.markerPatternData.markerPatternIDs[markerCounterRow];
			int markerPatternID2 = args.markerPatternData.markerPatternIDs[markerCounterColumn];

			singleMarkerPairData<maxAlleles>& markerPairData = computedContributions(markerPatternID1, markerPatternID2);
			//We only calculated tabels for markerPattern1 <= markerPattern2. So if we want things the other way around we have to swap the data for markers 1 and 2 later on. 
			bool swap = markerPatternID1 > markerPatternID2;
			for(int recombCounter = 0; recombCounter < nRecombLevels; recombCounter++)
			{
				for(int finalCounter = 0; finalCounter < nFinals; finalCounter++)
				{
					int marker1Value = args.finals(finalCounter, markerCounterRow);
					int marker2Value = args.finals(finalCounter, markerCounterColumn);
					//If necessary swap the data
					if(swap) std::swap(marker1Value, marker2Value);
					if(marker1Value != NA_INTEGER && marker2Value != NA_INTEGER)
					{
						double contribution = 0;
						bool allowable = false;
						int intercrossingGenerations = args.intercrossingGenerations[finalCounter];
						int selfingGenerations = args.selfingGenerations[finalCounter];
						if(intercrossingGenerations == 0)
						{
							funnelID currentLineFunnelID = args.funnelIDs[finalCounter];
							allowable = markerPairData.allowableFunnel(currentLineFunnelID, selfingGenerations - minSelfing);
							if(allowable)
							{
								array2<maxAlleles>& perMarkerGenotypeValues = markerPairData.perFunnelData(recombCounter, currentLineFunnelID, selfingGenerations - minSelfing);
								contribution = perMarkerGenotypeValues.values[marker1Value][marker2Value];
							}
						}
						else if(intercrossingGenerations > 0)
						{
							allowable = markerPairData.allowableAI(intercrossingGenerations-1, selfingGenerations - minSelfing);
							if(allowable)
							{
								array2<maxAlleles>& perMarkerGenotypeValues = markerPairData.perAIGenerationData(recombCounter, intercrossingGenerations-1, selfingGenerations-minSelfing);
								contribution = perMarkerGenotypeValues.values[marker1Value][marker2Value];
							}
						}
						//We get an NA from trying to take the logarithm of zero - That is, this parameter is completely impossible for the given data, so put in -Inf
						if(contribution != contribution || contribution == -std::numeric_limits<double>::infinity()) args.result[(long)counter *(long)nRecombLevels + (long)recombCounter] = -std::numeric_limits<double>::infinity();
						else if(contribution != 0 && allowable) args.result[(long)counter * (long)nRecombLevels + (long)recombCounter] += lineWeights[finalCounter] * contribution;
					}
				}
			}
#ifdef USE_OPENMP
			#pragma omp critical
#endif
			{
				progressCounter++;
			}
#ifdef USE_OPENMP
			if(omp_get_thread_num() == 0)
#endif
			{
				updateProgressCounter++;
				if(updateProgressCounter % 10 == 0) args.updateProgress(progressCounter);
			}
		}
	}
	return true;
}
template<int nFounders, int maxAlleles, bool infiniteSelfing> bool estimateRFSpecificDesignNoLineWeights(rfhaps_internal_args& args, unsigned long long& progressCounter)
{
	std::size_t nFinals = args.finals.nrow(), nRecombLevels = args.recombinationFractions.size();
	std::size_t nDifferentFunnels = args.funnelEncodings.size();
	std::vector<double>& lineWeights = args.lineWeights;
	Rcpp::List finalDimNames = args.finals.attr("dimnames");
	Rcpp::CharacterVector finalNames = finalDimNames[0];

	int nMarkerPatternIDs = (int)args.markerPatternData.allMarkerPatterns.size();
	int maxAIGenerations = *std::max_element(args.intercrossingGenerations.begin(), args.intercrossingGenerations.end());
	int minAIGenerations = *std::min_element(args.intercrossingGenerations.begin(), args.intercrossingGenerations.end());
	int minSelfing = *std::min_element(args.selfingGenerations.begin(), args.selfingGenerations.end());
	int maxSelfing = *std::max_element(args.selfingGenerations.begin(), args.selfingGenerations.end());

	//This is basically just a huge lookup table
	allMarkerPairData<maxAlleles> computedContributions(nMarkerPatternIDs);
	
	constructLookupTableArgs<maxAlleles, nFounders> lookupArgs(computedContributions, args.markerPatternData);
	lookupArgs.recombinationFractions = &args.recombinationFractions;
	lookupArgs.funnelEncodings = &args.funnelEncodings;
	lookupArgs.intercrossingGenerations = &args.intercrossingGenerations;
	lookupArgs.selfingGenerations = &args.selfingGenerations;
	constructLookupTable<nFounders, maxAlleles, infiniteSelfing>(lookupArgs);

	const int product1 = maxAlleles*(maxSelfing-minSelfing + 1) *(nDifferentFunnels + maxAIGenerations - minAIGenerations+1);
	const int product2 = (maxSelfing-minSelfing + 1) *(nDifferentFunnels + maxAIGenerations - minAIGenerations+1);
	const int product3 = nDifferentFunnels + maxAIGenerations - minAIGenerations+1;

	//We parallelise this array, even though it's over an iterator not an integer. So we use an integer and use that to work out how many steps forwards we need to move the iterator. We assume that the values are strictly increasing, otherwise this will never work.
	//Use this to only call setTxtProgressBar every 10 calls to updateProgress. Probably no point in updating status more frequently than that.
	unsigned long long updateProgressCounter = 0;
#ifdef USE_OPENMP
	#pragma omp parallel 
#endif
	{
		triangularIterator indexIterator = args.startPosition;
		//Indexing is of the form table[allele1 * product1 + allele2*product2 + selfingGenerations * product3 + (ai OR funnel)]. Funnels come first. 
		std::vector<int> table(maxAlleles*product1);

		int previousCounter = 0;
#ifdef USE_OPENMP
		#pragma omp for schedule(static, 1)
#endif
		for(int counter = 0; counter < args.valuesToEstimateInChunk; counter++)
		{
			std::fill(table.begin(), table.end(), 0);
			int difference = counter - previousCounter;
			if(difference < 0) throw std::runtime_error("Internal error");
			while(difference > 0) 
			{
				indexIterator.next();
				difference--;
			}
			previousCounter = counter;

			std::pair<int, int> markerIndices = indexIterator.get();
			int markerCounterRow = markerIndices.first, markerCounterColumn = markerIndices.second;

			int markerPatternID1 = args.markerPatternData.markerPatternIDs[markerCounterRow];
			int markerPatternID2 = args.markerPatternData.markerPatternIDs[markerCounterColumn];

			singleMarkerPairData<maxAlleles>& markerPairData = computedContributions(markerPatternID1, markerPatternID2);
			//We only calculated tabels for markerPattern1 <= markerPattern2. So if we want things the other way around we have to swap the data for markers 1 and 2 later on. 
			bool swap = markerPatternID1 > markerPatternID2;
			for(int finalCounter = 0; finalCounter < nFinals; finalCounter++)
			{
				int marker1Value = args.finals(finalCounter, markerCounterRow);
				int marker2Value = args.finals(finalCounter, markerCounterColumn);
				//If necessary swap the data
				if(swap) std::swap(marker1Value, marker2Value);
				if(marker1Value != NA_INTEGER && marker2Value != NA_INTEGER)
				{
					bool allowable = false;
					int intercrossingGenerations = args.intercrossingGenerations[finalCounter];
					int selfingGenerations = args.selfingGenerations[finalCounter];
					if(intercrossingGenerations == 0)
					{
						funnelID currentLineFunnelID = args.funnelIDs[finalCounter];
						table[marker1Value*product1 + marker2Value*product2 + (selfingGenerations - minSelfing)*product3 + currentLineFunnelID]++;
					}
					else if(intercrossingGenerations > 0)
					{
						table[marker1Value*product1 + marker2Value*product2 + (selfingGenerations - minSelfing)*product3 + nDifferentFunnels + intercrossingGenerations - minAIGenerations]++;
					}
				}
			}
			for(int recombCounter = 0; recombCounter < nRecombLevels; recombCounter++)
			{
				double contribution = 0;
				for(int selfingGenerations = minSelfing; selfingGenerations <= maxSelfing; selfingGenerations++)
				{
					for(int marker1Value = 0; marker1Value < maxAlleles; marker1Value++)
					{
						for(int marker2Value = 0; marker2Value < maxAlleles; marker2Value++)
						{
							for(int intercrossingGenerations = std::max(minAIGenerations,1); intercrossingGenerations <= maxAIGenerations; intercrossingGenerations++)
							{
								int count = table[marker1Value*product1 + marker2Value * product2 + (selfingGenerations - minSelfing)*product3 + nDifferentFunnels + intercrossingGenerations - minAIGenerations];
								if(count == 0) continue;
								bool allowable = markerPairData.allowableAI(intercrossingGenerations-1, selfingGenerations - minSelfing);
								if(allowable)
								{
									array2<maxAlleles>& perMarkerGenotypeValues = markerPairData.perAIGenerationData(recombCounter, intercrossingGenerations-1, selfingGenerations - minSelfing);
									contribution += count * perMarkerGenotypeValues.values[marker1Value][marker2Value];
								}
							}
							for(int funnelID = 0; funnelID < nDifferentFunnels; funnelID++)
							{
								int count = table[marker1Value*product1 + marker2Value * product2 + (selfingGenerations - minSelfing)*product3 + funnelID];
								if(count == 0) continue;
								bool allowable = markerPairData.allowableFunnel(funnelID, selfingGenerations - minSelfing);
								if(allowable)
								{
									array2<maxAlleles>& perMarkerGenotypeValues = markerPairData.perFunnelData(recombCounter, funnelID, selfingGenerations - minSelfing);
									contribution += count * perMarkerGenotypeValues.values[marker1Value][marker2Value];
								}
							}

						}
					}
				}
				//We get an NA from trying to take the logarithm of zero - That is, this parameter is completely impossible for the given data, so put in -Inf
				if(contribution != contribution || contribution == -std::numeric_limits<double>::infinity()) args.result[(long)counter *(long)nRecombLevels + (long)recombCounter] = -std::numeric_limits<double>::infinity();
				else args.result[(long)counter * (long)nRecombLevels + (long)recombCounter] += contribution;
			}
#ifdef USE_OPENMP
			#pragma omp critical
#endif
			{
				progressCounter++;
			}
#ifdef USE_OPENMP
			if(omp_get_thread_num() == 0)
#endif
			{
				updateProgressCounter++;
				if(updateProgressCounter % 10 == 0) args.updateProgress(progressCounter);
			}
		}
	}
	return true;
}
template<int nFounders, int maxAlleles, bool infiniteSelfing> bool estimateRFSpecificDesign3(rfhaps_internal_args& args, unsigned long long& counter)
{
	for(std::vector<double>::iterator i = args.lineWeights.begin(); i != args.lineWeights.end(); i++)
	{
		if(*i != 1) return estimateRFSpecificDesign<nFounders, maxAlleles, infiniteSelfing>(args, counter);
	}
	return estimateRFSpecificDesignNoLineWeights<nFounders, maxAlleles, infiniteSelfing>(args, counter);
}
template<int nFounders, int maxAlleles> bool estimateRFSpecificDesignInternal2(rfhaps_internal_args& args, unsigned long long& counter)
{
	bool infiniteSelfing = Rcpp::as<std::string>(args.pedigree.slot("selfing")) == "infinite";
	if(infiniteSelfing)
	{
		std::fill(args.selfingGenerations.begin(), args.selfingGenerations.end(), 0);
		return estimateRFSpecificDesign3<nFounders, maxAlleles, true>(args, counter);
	}
	else return estimateRFSpecificDesign3<nFounders, maxAlleles, false>(args, counter);
}
//here we transfer maxAlleles over to the templated parameter section - This can make a BIG difference to memory usage if this is smaller, and it's going into a type so it has to be templated.
template<int nFounders> bool estimateRFSpecificDesignInternal1(rfhaps_internal_args& args, unsigned long long& counter)
{
	switch(args.maxAlleles)
	{
		case 1:
			return estimateRFSpecificDesignInternal2<nFounders, 1>(args, counter);
		case 2:
			return estimateRFSpecificDesignInternal2<nFounders, 2>(args, counter);
		case 3:
			return estimateRFSpecificDesignInternal2<nFounders, 3>(args, counter);
		case 4:
			return estimateRFSpecificDesignInternal2<nFounders, 4>(args, counter);
		case 5:
			return estimateRFSpecificDesignInternal2<nFounders, 5>(args, counter);
		case 6:
			return estimateRFSpecificDesignInternal2<nFounders, 6>(args, counter);
		case 7:
			return estimateRFSpecificDesignInternal2<nFounders, 7>(args, counter);
		case 8:
			return estimateRFSpecificDesignInternal2<nFounders, 8>(args, counter);
		case 9:
			return estimateRFSpecificDesignInternal2<nFounders, 9>(args, counter);
		case 10:
			return estimateRFSpecificDesignInternal2<nFounders, 10>(args, counter);
		default:
			throw std::runtime_error("Internal error");
	}
}
unsigned long long estimateLookup(rfhaps_internal_args& internal_args)
{
	int maxAIGenerations = *std::max_element(internal_args.intercrossingGenerations.begin(), internal_args.intercrossingGenerations.end());
	int minSelfing = *std::min_element(internal_args.selfingGenerations.begin(), internal_args.selfingGenerations.end());
	int maxSelfing = *std::max_element(internal_args.selfingGenerations.begin(), internal_args.selfingGenerations.end());
	std::size_t nDifferentFunnels = internal_args.funnelEncodings.size();
	std::size_t nRecombLevels = internal_args.recombinationFractions.size();

	std::size_t arraySize;
	switch(internal_args.maxAlleles)
	{
		case 1:
			arraySize = sizeof(array2<1>);
			break;
		case 2:
			arraySize = sizeof(array2<2>);
			break;
		case 3:
			arraySize = sizeof(array2<3>);
			break;
		case 4:
			arraySize = sizeof(array2<4>);
			break;
		case 5:
			arraySize = sizeof(array2<5>);
			break;
		case 6:
			arraySize = sizeof(array2<6>);
			break;
		case 7:
			arraySize = sizeof(array2<7>);
			break;
		case 8:
			arraySize = sizeof(array2<8>);
			break;
		case 9:
			arraySize = sizeof(array2<9>);
			break;
		case 10:
			arraySize = sizeof(array2<10>);
			break;
		default:
			throw std::runtime_error("Internal error");
	}
	return nRecombLevels * (maxSelfing - minSelfing + 1) * (nDifferentFunnels + maxAIGenerations) * arraySize;
}
bool toInternalArgs(estimateRFSpecificDesignArgs&& args, rfhaps_internal_args& internal_args, std::string& error)
{
	error = "";
	std::stringstream ss;
	//work out the number of intercrossing generations
	int nFounders = args.founders.nrow(), nFinals = args.finals.nrow(), nMarkers = args.finals.ncol();
	std::vector<int> intercrossingGenerations, selfingGenerations;
	getIntercrossingAndSelfingGenerations(args.pedigree, args.finals, nFounders, intercrossingGenerations, selfingGenerations);
	bool hasAIC = *std::max_element(intercrossingGenerations.begin(), intercrossingGenerations.end()) > 0;

	/*Check that all the observed marker values are potentially valid (ignoring pedigree). That is, is every observed value for the finals consistent with something in the hetData object?*/
	Rcpp::List codingErrors = listCodingErrors(args.founders, args.finals, args.hetData);
	std::vector<std::string> warnings, errors;
	codingErrorsToStrings(codingErrors, errors, args.finals, Rcpp::as<Rcpp::List>(args.hetData), 6);
	for(std::size_t errorIndex = 0; errorIndex < errors.size() && errorIndex < 6; errorIndex++)
	{
		ss << errors[errorIndex] << std::endl;
	}
	if(errors.size() > 0)
	{
		error = ss.str();
		return false;
	}

	//Check that everything has a unique funnel - For the case of the lines which are just selfing, we just check that one funnel. For AIC lines, we check the funnels of all the parent lines
	std::vector<funnelType> allFunnels;
	{
		estimateRFCheckFunnels(args.finals, args.founders,  Rcpp::as<Rcpp::List>(args.hetData), args.pedigree, intercrossingGenerations, warnings, errors, allFunnels);
		for(std::size_t errorIndex = 0; errorIndex < errors.size() && errorIndex < 6; errorIndex++)
		{
			ss << errors[errorIndex] << std::endl;;
		}
		if(errors.size() > 0)
		{
			error = ss.str();
			return false;
		}
		for(std::size_t warningIndex = 0; warningIndex < warnings.size() && warningIndex < 6; warningIndex++)
		{
			Rprintf(warnings[warningIndex].c_str());
		}
		if(warnings.size() > 6)
		{
			Rprintf("Supressing further funnel warnings");
		}
	}
	//re-code the founder and final marker genotypes so that they always start at 0 and go up to n-1 where n is the number of distinct marker alleles
	//We do this to make it easier to identify markers with identical segregation patterns. recodedFounders = column major matrix
	Rcpp::IntegerMatrix recodedFounders(nFounders, nMarkers), recodedFinals(nFinals, nMarkers);
	Rcpp::List recodedHetData(nMarkers);
	recodedHetData.attr("names") = args.hetData.attr("names");
	recodedFinals.attr("dimnames") = args.finals.attr("dimnames");
	
	recodeDataStruct recoded;
	recoded.recodedFounders = recodedFounders;
	recoded.recodedFinals = recodedFinals;
	recoded.founders = args.founders;
	recoded.finals = args.finals;
	recoded.hetData = args.hetData;
	recoded.recodedHetData = recodedHetData;
	recodeFoundersFinalsHets(recoded);

	unsigned int maxAlleles = recoded.maxAlleles;
	if(maxAlleles > 10)
	{
		error = "Internal error - Cannot have more than ten alleles per marker";
		return false;
	}

	//We need to assign a unique ID to each marker pattern - Where by pattern we mean the combination of hetData and founder alleles. Note that this is possible because we just recoded everything to a standardised format.
	//Marker IDs are guaranteed to be contiguous numbers starting from 0 - So the set of all valid [0, markerPatterns.size()]. 
	//Note that markerPatternID is defined in unitTypes.hpp. It's just an integer (and automatically convertible to an integer), but it's represented by a unique type - This stops us from confusing it with an ordinary integer.
	markerPatternsToUniqueValuesArgs markerPatternConversionArgs;
	markerPatternConversionArgs.nFounders = nFounders;
	markerPatternConversionArgs.nMarkers = nMarkers;
	markerPatternConversionArgs.recodedFounders = recodedFounders;
	markerPatternConversionArgs.recodedHetData = recodedHetData;
	markerPatternsToUniqueValues(markerPatternConversionArgs);
	
	//map containing encodings of the funnels involved in the experiment (as key), and an associated unique index (again, using the encoded values directly is no good because they'll be all over the place). Unique indices are contiguous again.
	std::map<funnelEncoding, funnelID> funnelTranslation;
	//vector giving the funnel ID for each individual
	std::vector<funnelID> funnelIDs;
	//vector giving the encoded value for each individual
	std::vector<funnelEncoding> funnelEncodings;
	funnelsToUniqueValues(funnelTranslation, funnelIDs, funnelEncodings, allFunnels, nFounders);
	
	//In the case of infinite selfing, we've still allowed hets up to this point. But we want to ignore any potential hets in the final analysis. 
	bool infiniteSelfing = Rcpp::as<std::string>(args.pedigree.slot("selfing")) == "infinite";
	if(infiniteSelfing)
	{
		bool foundHets = replaceHetsWithNA(recodedFounders, recodedFinals, recodedHetData);
		if(foundHets)
		{
			Rcpp::Function warning("warning");
			//Technically a warning could lead to an error if options(warn=2). This would be bad because it would break out of our code. This solution generates a c++ exception in that case, which we can then ignore. 
			try
			{
				warning("Input data had hetrozygotes but was analysed assuming infinite selfing. All hetrozygotes were ignored. \n");
			}
			catch(...)
			{}
		}
		std::fill(selfingGenerations.begin(), selfingGenerations.end(), 0);
	}
	
	internal_args.finals = recodedFinals;
	internal_args.founders = recodedFounders;
	internal_args.pedigree = args.pedigree;
	internal_args.intercrossingGenerations.swap(intercrossingGenerations);
	internal_args.selfingGenerations.swap(selfingGenerations);
	internal_args.lineWeights.swap(args.lineWeights);
	internal_args.markerPatternData.swap(markerPatternConversionArgs);
	internal_args.hasAI = hasAIC;
	internal_args.maxAlleles = maxAlleles;
	internal_args.funnelIDs.swap(funnelIDs);
	internal_args.funnelEncodings.swap(funnelEncodings);
	return true;
}
bool estimateRFSpecificDesign(rfhaps_internal_args& internal_args, unsigned long long& counter)
{
	int nFounders = internal_args.founders.nrow();
	if(nFounders == 2)
	{
		return estimateRFSpecificDesignInternal1<2>(internal_args, counter);
	}
	else if(nFounders == 4)
	{
		return estimateRFSpecificDesignInternal1<4>(internal_args, counter);
	}
	/*else if(nFounders == 8)
	{
		return estimateRFSpecificDesignInternal1<8>(internal_args);
	}*/
	else
	{
		Rprintf("Number of founders must be 2, 4 or 8\n");
		return false;
	}
	return true;
}
