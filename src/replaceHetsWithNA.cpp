#include "recodeHetsAsNA.h"
bool replaceHetsWithNA(Rcpp::IntegerMatrix recodedFounders, Rcpp::IntegerMatrix recodedFinals, Rcpp::List recodedHetData)
{
	int nMarkers = recodedFounders.ncol();
	int nFinals = recodedFinals.nrow();
	bool retValue = false;
	std::vector<bool> isHet(100);
	for(int markerCounter = 0; markerCounter < nMarkers; markerCounter++)
	{
		Rcpp::IntegerMatrix currentMarkerHetData = recodedHetData(markerCounter);
		std::fill(isHet.begin(), isHet.end(), true);
		for(int hetDataRowCounter = 0; hetDataRowCounter < currentMarkerHetData.nrow(); hetDataRowCounter++)
		{
			if(currentMarkerHetData(hetDataRowCounter, 0) == currentMarkerHetData(hetDataRowCounter, 1))
			{
				isHet[currentMarkerHetData(hetDataRowCounter, 2)] = false;
			}
		}
		for(int finalCounter = 0; finalCounter < nFinals; finalCounter++)
		{
			int finalValue = recodedFinals(finalCounter, markerCounter);
			if(finalValue != NA_INTEGER && isHet[finalValue])
			{
				recodedFinals(finalCounter, markerCounter) = NA_INTEGER;
				retValue = true;
			}
		}
	}
	return retValue;
}
