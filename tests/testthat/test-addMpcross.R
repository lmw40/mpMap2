context("Test addition of mpcross objects")

test_that("Cannot combine object with itself",
{
	map <- sim.map(len = 100, n.mar = 11, anchor.tel=TRUE, include.x=FALSE, eq.spacing=TRUE)
	f2Pedigree <- f2Pedigree(500)
	cross <- simulateMPCross(map=map, pedigree=f2Pedigree, mapFunction = haldane)
	expect_that(cross+cross, throws_error())
})
