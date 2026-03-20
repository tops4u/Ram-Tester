## These Documents describe the Function and Procedure to test 4164/3732/4532 RAM

32K has its name because half of good 4164 (64K times 1 bit) are 32K bits in size. 

1. Check if your firmware has the 32K option enabled in order to test 3732 and 4532 RAM -> See Docs Page
2. Once you insert any of the above RAM into the tester, there are various possibilities of defects. 
3. For a 4164, if no defects are found, it is reported as 4164 64K x 1 4ms or 2ms. If errors are found, it very much depends on where they are. If this RAM could still be used as 3732 or 4532, the tester will show which one would still work. Only if there are too many or too severe defects will a 4164 RAM show as a defect. So you will need to pay attention when reading the result for 4164 when 32K support is enabled. 
4. When testing 3732 or 4532, they will be tested in all quadrants (check the docs above), and if 3 quadrants are okay and all defects are just in one quadrant, a 3732 may also be used as 4532 and vice versa. Explanation see below.
5. When testing 3732 or 4532 and they show defects in two neighbouring quadrants, they only serve the type which is printed on the RAM. So only one specific subtype will be reported okay like: "3732-L 32K". 

### What is the history behind 3732 and 4532? 
In the early days, RAM was expensive, and 4164 RAM was just invented, and all computer manufacturers wanted it. Production was not perfect, and many silicon chips had some errors. In order to still use them, 2 companies (OKI and TI) had the idea to relabel them as 32K x 1 RAM by labeling them 3732 and 4532 respectively instead of 4164. They just did use a different approach. Both decided to split the available 64K in 2 by means of address line A7, so instead of 256 rows or columns, those RAMs only had 128. But here comes the twist... while TI decided to split the rows, like a horizontal split in lower and upper rows, OKI decided to split by columns, having like a left and right side (lower and upper columns). So those two were very incompatible. The only computer I know ever to use them was the ZX Spectrum for the higher part of its RAM. But you had to populate with the same type and subtype and define the behaviour with jumpers. So this led to 4 different RAM chips.

- OKI MSM3732-L having the lower columns working
- OKI MSM3732-H having the upper columns working
- TI TMS4532-NL3 having the lower rows working
- TI TMS4532-NL4 having the upper rows working

As you can see, if we mix the two concepts, this will give us 4 quadrants (imagine the RAM like a spreadsheet having 256 rows and 256 columns and draw lines in the middle at row 128 and col 128). If now the MSM3732-H has an error in the first quadrant (let’s say row 5, col 15), it would not be able to use the lower columns, thus type -H. But the very same would also apply to the TMS4532-4, just that this one would use all columns but only rows 128-256, also leaving out the error in quadrant 1. So this MSM3732-H could also serve as TMS4532-4. And this is what the 32K test of this tester will report as a result. 
