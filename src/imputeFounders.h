#ifndef IMPUTE_FOUNDERS_HEADER_GUARD
#define IMPUTE_FOUNDERS_HEADER_GUARD
#include "Rcpp.h"
SEXP imputeFounders(SEXP geneticData_sexp, SEXP map_sexp, SEXP homozygoteMissingProb_sexp, SEXP hetrozygoteMissingProb_sexp);
#endif
