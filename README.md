INTEL-AYC-2013SUMMER--2ND-PLACE-SOLUTION
========================================

Author: Dimitris Vlachos
Age: 25
Location: Athens,Greece
Email: DimitrisV22@gmail.com
Git: https://github.com/DimitrisVlachos




[Algorithm description - Intro]
Before i begin the in depth explanation of my solution , i would like to point out that my solution is focused mostly in the actual algorithm optimization department, with some additional algorithmic improvements , such as support for fixed angle rotations(90o,180o,270o) and multithreaded texture upscaling.




[Additional build configuration flags]
To disable fixed angle rotation pattern matching , add the following flag to the build script : 
-D_NO_FIXED_ANGLE_ROTATIONS

That will make the algorithm 3x times faster since only the identity pattern will be handled...




[Optimizing resource loading]
Since we don't have dedicated hardware that does all the magic behind the scenes , these are  the most obvious ways to improve the loading of the resources : 

0.Using the low level unix api : 
-5 syscalls(open,seek,seek,read,close) 
+Direct multiple sector copy to the buffer (No local IO buffer temporal copies)

1.Using the low level unix api combined with mmap : 
*Same as #0 but without the additional overhead of transfering the data to work ram

2.Using the low level unix api combined with mmap (w/ threads): 
*Same as #1 but the overhead is reduced by N# number of threads

3.Using the standard C/C++ io library : 
+No overhead from syscalls
-Multiple sector copy to IO library work buffer + additional transfer to work ram

Obviously , the 3rd method is the slowest for larger reads , but faster for multiple smaller reads from different file streams , but 
 for the type of the problem that we're going to deal with ,all other methods outperform it ,  with 2nd method beeing the fastest.

My solution implements #0,#1,#2 and also implements 64bit streams.




[Intermmediate representation of textures & reduced memory usage]
Texture file formats may vary , but in the end they're just a flat pixel chunk surface laid in (usually)ram , 
so the easiest way to reduce memory usage and simplify the pattern matching algorithm, is to simply create an intermediate representation of the texture by converting each pixel to grayscale (using a lookup table) 
 and to end up from a flat surface of (Width * Height * Bytes per pixel) to (Width * Height).




[Optimizing memory access]
-To deal with cache misses all memory blocks are perfectly aligned , and the all the actual heap addresses are 
patched on the fly to the next aligned offset.

-To deal with false sharing(threading) all shared contexts are aligned , and also for good programming practice all input/output variable types that are 
passed through the entry point call are moved to local variables.




[Optimizing pattern matching]
The search algorithm is split in 2x passes.

Block cmp pass : 
Comparison is performed by block runs(up to maximum bits a cpu register can hold) and all possible matches are stored temporarily and moved to the confirmation pass.

Confirmation pass :
Block runs that contain remainder must be validated byte per byte.
The loop iterations are guaranteed to be : height * (width - (block_size * nb_blocks))

Which is really anything  between : height * (1 ...(cpu.reg_size_bits/8)-1)



[Optimizing pattern matching : Line jumps & Pixel R/W operations by addition]
To access a pixel one would normaly use the following expression :
pixel = pixel_data[y * stride + (x * bpp)] ( where stride = width * bpp )  

But in my solution all pixel R/W operations are performed by addition, which greatly improve the algorithm's execution time.



[Optimizing pattern matching : SSE# jump tables]
The SSE# parallel version of the algorithm attempts to find missmatches in blocks of 16bytes &
based on the mask result , a lookup table is used to jump the closest missmatch within the 16byte relative offset.

Here's an example run cycle of how this works: 

Find : "A" in "00000000000000000000000000000000A00000000000000"

Iteration 0 (offs 0/48) : 16bit mask  {all bits off} {Jump = 16} { Offs += Jump  } 
Iteration 1 (offs 16/48): 16bit mask {all bits off}} {Jump = 16} { Offs += Jump  } 
Iteration 2 (offs 32/48): 16bit mask {first bit on , rest off} {Jump = 0}  { Offs += Jump  }  (Found!)




[Optimizing pattern matching : Thread scheduling]

The threading scheduling goes as follows :

repeat for all fixed angles(0,90,180,270) :
0.Subdivide work by height / nb_threads
1.scale 
2.rotate
3.find matches




[Additional parallel optimizations : MT Upscaler]
The mt upscaler works as follows:

0.Subdivide new texture height by nb_threads
1.Scale  each fraction in parallel
