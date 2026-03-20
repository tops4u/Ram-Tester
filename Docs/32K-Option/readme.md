# These Documents describe the Function and Procedure to test 4164/3732/4532 RAM

32K has it's name because half good 4164 (64K times 1 Bit) are 32K bits in size. 

1. Check if your Firmware has 32K Option enabled in order to Test 3732 and 4532 RAM -> See Docs Page
2. Once you insert any of the above RAM into the Tester there are various possibilites of defects. 
3. For a 4164 if no defects are found it is reported as 4164 64K x 1 4ms or 2ms. If Errors are found it very much depends where they are. If this RAM could still be used as 3732 or 4532, the Tester will show which one would still work. Only if there are too many or too severe defects a 4164 RAM will show as defect. So you will need to pay attention when reading the result for 4164, when 32K Support is enabled. 
4. When Testing 3732 or 4532 they will be tested in all quadrants (check the docs above) and if 3 quadrants are ok, and all defects are just in one quadrant, a 3732 may also be used as 4532 and vice-versa. Explanation see below.
5. When Testing 3732 or 4532 and they show defects in two neighbouring quadrants they only serve the type which is printed on the RAM. So only one specific Subtype will be reported ok like: "3732-L 32K". 

# What is the history behind 3732 and 4532? 
In the early days RAM was expensive and 4164 RAM was just invented and all computer manufacturers wanted it. Production was not perfect and many silicon chips had some errors. In order to still use them 2 companies (OKI and TI) had the idea to relabel them as 32K x 1 RAM by label them 3732 and 4532 respectively instead of 4164. Just they did use a different approach. Both decided to split the available 64K in 2 by means of addressline A7 so instead of 256 Rows or Columns those RAMs only had 128. But here comes the Twist... while TI decided to split the Rows, like a horizontal Split in lower and upper Rows, OKI decided to split by Columns having like a left and right side (lower and upper Columns). So those two were very incompatible. The only Computer I know ever to use them was the ZX Spektrum for the higher Part of its RAM. But you had to poplate with the same Type and Subtype and define the behaviour with Jumpers. So this lead to 4 different RAM Chips.

OKI MSM3732-L having the Lower Columns working
OKI MSM3732-H having the Upper Columns working
TI TMS4532-NL3 having the Lower Rows working
TI TMS4532-NL4 having the Upper Rows working

As you can see if we mix the two concepts this will give us 4 Quadrants (imagine the RAM like an Spreadsheet having 256 Rows and 256 Columns and draw lines in middle at row 128 and col 128). If now you MSM3732-H has an Error in the first Quadrant (lets say Row 5, Col 15) it would not be able to use the lower columns thus type -H. But the very same would also apply to the TMS4532-4 just that this one would use all Columns but only Rows 128-256 also leaving out the Erorr in Quadrant 1. So this MSM3732-H could also serve as TMS4532-4. And this is what the 32K Test of this tester will report as result. 
