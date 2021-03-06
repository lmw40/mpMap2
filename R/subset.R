#' @export
setMethod(f = "subset", signature = "mpcross", definition = function(x, ...)
{
	arguments <- list(...)
	if(sum(c("markers", "lines") %in% names(arguments)) != 1)
	{
		stop("Exactly one of arguments markers and lines is required for function subset.mpcross")
	}
	if("lines" %in% names(arguments))
	{
		if(mode(arguments$lines) == "numeric")
		{
			if(length(x@geneticData) != 1)
			{
				stop("Argument lines cannot be a vector of indices if there is more than one data set")
			}
			arguments$lines <- rownames(x@geneticData[[1]]@finals)[arguments$lines]
		}
	}
	if("markers" %in% names(arguments))
	{
		if(mode(arguments$markers) == "character")
		{
			if(any(!(arguments$markers %in% markers(x))))
			{
				stop("Not all named markers were contained in the input object")
			}
		}
		else if(mode(arguments$markers) == "numeric")
		{
			if(any(arguments$markers < 1) || any(arguments$markers > nMarkers(x)))
			{
				stop("Input marker indices were out of range")
			}
		}
		else
		{
			stop("Input markers must be either a vector of indices or a vector of marker names")
		}
	}
	newGeneticData <- lapply(x@geneticData, 
		function(geneticData)
		{
			do.call(subset, c(arguments, x = geneticData))
		})
	return(new("mpcross", geneticData = new("geneticDataList", newGeneticData)))
})
setMethod(f = "subset", signature = "mpcrossMapped", definition = function(x, ...)
{
	subsettedRF <- NULL
	if(!is.null(x@rf)) subsettedRF <- subset(x@rf, ...)
	return(new("mpcrossMapped", callNextMethod(), map = subset(x@map, ...), rf = subsettedRF))
})
setMethod(f = "subset", signature = "mpcrossRF", definition = function(x, ...)
{
	return(new("mpcrossRF", callNextMethod(), "rf" = subset(x@rf, ...)))
})
setMethod(f = "subset", signature = "mpcrossLG", definition = function(x, ...)
{
	arguments <- list(...)
	if(sum(c("groups", "markers", "lines") %in% names(arguments)) != 1)
	{
		stop("Exactly one of arguments markers, lines and groups is required for function subset.mpcross")
	}
	if("groups" %in% names(arguments) && length(arguments$groups) != length(unique(arguments$groups)))
	{
		stop("Duplicates detected in argument groups of subset function")
	}
	if("lines" %in% names(arguments) && length(arguments$lines) != length(unique(arguments$lines)))
	{
		stop("Duplicates detected in argument lines of subset function")
	}
	if("markers" %in% names(arguments) && length(arguments$markers) != length(unique(arguments$markers)))
	{
		stop("Duplicates detected in argument markers of subset function")
	}
	if("groups" %in% names(arguments))
	{
		if(!all(arguments$groups %in% x@lg@allGroups))
		{
			stop("Input groups must be a subset of the values in x@lg@allGroups")
		}
		markerIndices <- which(x@lg@groups %in% arguments$groups)
		markers <- markers(x)[markerIndices]
		subsettedRF <- NULL
		if(!is.null(x@rf)) subsettedRF <- subset(x@rf, markers = markers)
		return(new("mpcrossLG", callNextMethod(x, markers = markers), "lg" = subset(x@lg, markers = markers), "rf" = subsettedRF))
	}
	else
	{
		subsettedRF <- NULL
		if(!is.null(x@rf)) subsettedRF <- subset(x@rf, ...)
		return(new("mpcrossLG", callNextMethod(), "lg" = subset(x@lg, ...), "rf" = subsettedRF))
	}
})
setMethod(f = "subset", signature = "lg", definition = function(x, ...)
{
	arguments <- list(...)
	if(!("markers" %in% names(arguments)))
	{
		stop("Argument markers is required for function subset.lg")
	}
	if("markers" %in% names(arguments) && length(arguments$markers) != length(unique(arguments$markers)))
	{
		stop("Duplicates detected in argument markers of subset function")
	}

	markers <- arguments$markers
	if(mode(markers) == "numeric")
	{
		markerIndices <- markers
		markerNames <- markers(x)[markerIndices]
	}
	else if(mode(markers) == "character")
	{
		markerIndices <- match(markers, markers(x))
		markerNames <- markers
	}

	groups <- x@groups[markerIndices]
	allGroups <- sort(unique(groups))
	imputedTheta <- NULL
	if(!is.null(x@imputedTheta))
	{
		imputedTheta <- vector(mode = "list", length = length(allGroups))
		names(imputedTheta) <- as.character(allGroups)
		sapply(as.character(allGroups), function(groupCharacter) imputedTheta[[groupCharacter]] <<- subset(x@imputedTheta[[groupCharacter]], markers = intersect(markerNames, names(which(x@groups == as.integer(groupCharacter))))), simplify = FALSE)
	}
	return(new("lg", groups = groups, allGroups = allGroups, imputedTheta = imputedTheta))
})
setMethod(f = "subset", signature = "geneticData", definition = function(x, ...)
{
	arguments <- list(...)
	if(sum(c("markers", "lines") %in% names(arguments)) != 1)
	{
		stop("Exactly one of arguments markers and lines is required for function subset.geneticData")
	}
	if("lines" %in% names(arguments) && length(arguments$lines) != length(unique(arguments$lines)))
	{
		stop("Duplicates detected in argument lines of subset function")
	}
	if("markers" %in% names(arguments) && length(arguments$markers) != length(unique(arguments$markers)))
	{
		stop("Duplicates detected in argument markers of subset function")
	}

	if("markers" %in% names(arguments))
	{
		markers <- arguments$markers
		if(mode(markers) == "numeric")
		{
			markerIndices <- markers
		}
		else if(mode(markers) == "character")
		{
			markerIndices <- match(markers, markers(x))
		}
		else stop("Input markers must be either a vector of marker names or a vector of marker indices")
	
		return(new("geneticData", founders = x@founders[,markerIndices,drop=FALSE], finals = x@finals[,markerIndices,drop=FALSE], hetData = subset(x@hetData, ...), pedigree = x@pedigree))
	}
	else
	{
		lines <- arguments$lines
		if(mode(lines) == "numeric")
		{
			lines <- rownames(x@finals)[lines]
		}
		else if(mode(lines) != "character")
		{
			stop("Input lines must be a vector of line names")
		}
		return(new("geneticData", founders = x@founders, finals = x@finals[rownames(x@finals)%in% lines,,drop=FALSE], hetData = x@hetData, pedigree = as(x@pedigree, "pedigree")))
	}
})
setMethod(f = "subset", signature = "hetData", definition = function(x, ...)
{
	arguments <- list(...)
	if(!("markers" %in% names(arguments)))
	{
		stop("Argument markers is required for function subset.hetData")
	}
	if("markers" %in% names(arguments) && length(arguments$markers) != length(unique(arguments$markers)))
	{
		stop("Duplicates detected in argument markers of subset function")
	}

	markers <- arguments$markers
	if(mode(markers) == "numeric")
	{
		markerIndices <- markers
	}
	else if(mode(markers) == "character")
	{
		markerIndices <- match(markers, markers(x))
	}
	return(new("hetData", x[markerIndices]))
})
setMethod(f = "subset", signature = "rf", definition = function(x, ...)
{
	arguments <- list(...)
	if(!("markers" %in% names(arguments)))
	{
		stop("Argument markers is required for function subset.rf")
	}
	if("markers" %in% names(arguments) && length(arguments$markers) != length(unique(arguments$markers)))
	{
		stop("Duplicates detected in argument markers of subset function")
	}

	markers <- arguments$markers
	if(mode(markers) == "numeric")
	{
		markerIndices <- markers
	}
	else if(mode(markers) == "character")
	{
		markerIndices <- match(markers, markers(x))
	}
	
	newTheta <- subset(x@theta, markers = markerIndices)
	
	if(is.null(x@lod))
	{
		newLod <- NULL
	}
	else
	{
		newLod <- x@lod[markerIndices, markerIndices,drop=FALSE]
	}

	if(is.null(x@lkhd))
	{
		newLkhd <- NULL
	}
	else
	{
		newLkhd <- x@lkhd[markerIndices, markerIndices,drop=FALSE]
	}
	return(new("rf", theta = newTheta, lod = newLod, lkhd = newLkhd, gbLimit = x@gbLimit))
})
setMethod(f = "subset", signature = "rawSymmetricMatrix", definition = function(x, ...)
{
	arguments <- list(...)
	if(!("markers" %in% names(arguments)) || length(arguments) > 1)
	{
		stop("Only argument markers is allowed for function subset.rawSymmetricMatrix")
	}
	if("markers" %in% names(arguments) && length(arguments$markers) != length(unique(arguments$markers)))
	{
		stop("Duplicates detected in argument markers of subset function")
	}

	markers <- arguments$markers
	if(is.character(markers)) 
	{	
		markers <- match(markers, x@markers)
		if(any(is.na(markers)))
		{
			stop("Invalid marker names entered in function subset.rawSymetricMatrix")
		}
	}
	markers <- as.integer(markers)
	if(any(is.na(markers)))
	{
		stop("Marker indices cannot be NA in function subset.rawSymmetricMatrix")
	}
	if(any(markers < 1) || any(markers > length(x@markers)))
	{
		stop("Input marker indices were out of range in function subset.rawSymmetricMatrix")
	}
	newRawData <- .Call("rawSymmetricMatrixSubsetObject", x, markers, PACKAGE="mpMap2")
	retVal <- new("rawSymmetricMatrix", data = newRawData, markers = x@markers[markers], levels = x@levels)
	return(retVal)
})
