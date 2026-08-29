#include <assert.h>
#include <string.h>
#include <algorithm>
#include <thread>
#define NOMINMAX // Need to include this to stop windows.h (below) breaking std::min etc
#include <Windows.h>

#include "GlobalTypes.h"
#include "Engine.h"
#include "UGI.h"
#include "Brain.h"
#include "Utilities.h"
#include "SearchPerft.h"

//----------------------------------------------------------------------------------------------------


//#include <stdio.h>
//#include <stdint.h>
//
//// Structure to represent a 128-bit integer
//typedef struct {
//	uint64_t low;  // Lower 64 bits
//	uint64_t high; // Upper 64 bits
//} uint128_t;
//
//// Function to add two 128-bit numbers
//uint128_t add128(uint128_t a, uint128_t b) {
//	uint128_t result;
//
//	// Add the lower 64 bits
//	result.low = a.low + b.low;
//
//	// Check for carry from lower 64-bit addition
//	uint64_t carry = (result.low < a.low) ? 1 : 0;
//
//	// Add the higher 64 bits with carry
//	result.high = a.high + b.high + carry;
//
//	return result;
//}
//
//// Function to print a 128-bit number
//void print128(uint128_t num) {
//	printf("0x%016llx%016llx\n", num.high, num.low);
//}
//
//int main() {
//	// Example usage
//	uint128_t num1 = { 0xFFFFFFFFFFFFFFFF, 0x0000000000000001 }; // 2^64
//	uint128_t num2 = { 0x1, 0x0 }; // 1
//
//	uint128_t sum = add128(num1, num2);
//
//	printf("Sum: ");
//	print128(sum);
//
//	return 0;
//}





//Hash=64MB
//P1:1/0
//P2:1/0
//P3:1/0
//P4:1/0
//P5:8/1
//P6:115/16
//P7:1241/204
//P8:15176/997
//P9:324905/1000
//Hash=1024MB
//P1:1/0
//P2:1/0
//P3:1/0
//P4:1/0
//P5:9/0
//P6:117/1
//P7:1263/16
//P8:14601/153
//P9:175244/957


// 4.4 hours (4GB hash, 8 threads, AMD Ryzen 5 3600 6-Core Processor 3.59 GHz)
//debug on
//info string Debug set to true
//setoption name hash value 4096
//info string Transposition table memory set to 4096MB
//setoption name threads value 8
//info string Threads set to 8
//
//go perft 11
//info string Transposition table memory = 4096MB(4294967296 bytes)
//info string Perft transposition table bucket size = 64 bytes
//info string Perft transposition table entry size = 16 bytes
//info string Perft transposition table entries per bucket = 4
//info string Perft transposition table buckets = 67108864
//info string Perft transposition table entries = 268435456
//info string Perft transposition table memory allocated = 4096MB(4294967296 bytes)
//info string a2a3 : 60403292887824 (6690482ms, Hashfull = 1000, ThreadId = 0)
//info string b1a3 : 70080800068168 (7629790ms, Hashfull = 1000, ThreadId = 3)
//info string g1f3 : 89933046388964 (7904013ms, Hashfull = 1000, ThreadId = 1)
//info string g2g4 : 73966186324024 (8312945ms, Hashfull = 1000, ThreadId = 5)
//info string c2c3 : 92235553734553 (8504728ms, Hashfull = 1000, ThreadId = 2)
//info string g2g3 : 82762826570051 (8747564ms, Hashfull = 1000, ThreadId = 6)
//info string b2b3 : 79510326025357 (5401340ms, Hashfull = 1000, ThreadId = 0)
//info string c2c3 : 92235553734553 (30ms, Hashfull = 1000, ThreadId = 0)
//info string h2h3 : 60097879424719 (3676513ms, Hashfull = 1000, ThreadId = 6)
//info string b1c3 : 91451554526572 (4837121ms, Hashfull = 1000, ThreadId = 1)
//info string b1a3 : 70080800068168 (26ms, Hashfull = 1000, ThreadId = 1)
//info string f2f4 : 68372448303691 (4524312ms, Hashfull = 1000, ThreadId = 5)
//info string h2h4 : 86739921618220 (180219ms, Hashfull = 1000, ThreadId = 1)
//info string h2h4 : 86739921618220 (5291591ms, Hashfull = 1000, ThreadId = 3)
//info string g2g4 : 73966186324024 (27ms, Hashfull = 1000, ThreadId = 1)
//info string g2g4 : 73966186324024 (26ms, Hashfull = 1000, ThreadId = 3)
//info string f2f4 : 68372448303691 (27ms, Hashfull = 1000, ThreadId = 1)
//info string f2f4 : 68372448303691 (26ms, Hashfull = 1000, ThreadId = 3)
//info string e2e4 : 245841494675197 (1137969ms, Hashfull = 1000, ThreadId = 1)
//info string e2e4 : 245841494675197 (1137974ms, Hashfull = 1000, ThreadId = 3)
//info string e2e4 : 245841494675197 (1222154ms, Hashfull = 1000, ThreadId = 5)
//info string e2e4 : 245841494675197 (14059416ms, Hashfull = 1000, ThreadId = 7)
//info string d2d3 : 151857971385067 (2488933ms, Hashfull = 1000, ThreadId = 0)
//info string d2d3 : 151857971385067 (6076061ms, Hashfull = 1000, ThreadId = 2)
//info string a2a4 : 85054341127064 (2568107ms, Hashfull = 1000, ThreadId = 6)
//info string e2e3 : 241074613621302 (15218530ms, Hashfull = 1000, ThreadId = 4)
//info string e2e3 : 241074613621302 (637794ms, Hashfull = 1000, ThreadId = 2)
//info string e2e3 : 241074613621302 (637810ms, Hashfull = 1000, ThreadId = 0)
//info string f2f3 : 51614296095395 (272047ms, Hashfull = 1000, ThreadId = 0)
//info string f2f3 : 51614296095395 (272115ms, Hashfull = 1000, ThreadId = 4)
//info string g2g3 : 82762826570051 (26ms, Hashfull = 1000, ThreadId = 0)
//info string f2f3 : 51614296095395 (272090ms, Hashfull = 1000, ThreadId = 2)
//info string g2g3 : 82762826570051 (28ms, Hashfull = 1000, ThreadId = 4)
//info string h2h3 : 60097879424719 (20ms, Hashfull = 1000, ThreadId = 0)
//info string h2h3 : 60097879424719 (19ms, Hashfull = 1000, ThreadId = 4)
//info string g2g3 : 82762826570051 (29ms, Hashfull = 1000, ThreadId = 2)
//info string a2a4 : 85054341127064 (29ms, Hashfull = 1000, ThreadId = 4)
//info string h2h3 : 60097879424719 (22ms, Hashfull = 1000, ThreadId = 2)
//info string a2a4 : 85054341127064 (57ms, Hashfull = 1000, ThreadId = 0)
//info string a2a4 : 85054341127064 (28ms, Hashfull = 1000, ThreadId = 2)
//info string b2b4 : 80419308561211 (103499ms, Hashfull = 1000, ThreadId = 0)
//info string b2b4 : 80419308561211 (103496ms, Hashfull = 1000, ThreadId = 2)
//info string b2b4 : 80419308561211 (103531ms, Hashfull = 1000, ThreadId = 4)
//info string b2b4 : 80419308561211 (602067ms, Hashfull = 1000, ThreadId = 6)
//info string d2d4 : 211583204457112 (1665825ms, Hashfull = 1000, ThreadId = 1)
//info string d2d4 : 211583204457112 (1665821ms, Hashfull = 1000, ThreadId = 3)
//info string d2d4 : 211583204457112 (1665820ms, Hashfull = 1000, ThreadId = 5)
//info string d2d4 : 211583204457112 (1665819ms, Hashfull = 1000, ThreadId = 7)
//info string c2c4 : 103605670223681 (110605ms, Hashfull = 1000, ThreadId = 1)
//info string c2c4 : 103605670223681 (241587ms, Hashfull = 1000, ThreadId = 0)
//info string c2c4 : 103605670223681 (241587ms, Hashfull = 1000, ThreadId = 2)
//info string c2c4 : 103605670223681 (110605ms, Hashfull = 1000, ThreadId = 3)
//info string c2c4 : 103605670223681 (241587ms, Hashfull = 1000, ThreadId = 4)
//info string c2c4 : 103605670223681 (110606ms, Hashfull = 1000, ThreadId = 5)
//info string c2c4 : 103605670223681 (110603ms, Hashfull = 1000, ThreadId = 7)
//info string c2c4 : 103605670223681 (241590ms, Hashfull = 1000, ThreadId = 6)
//info string b2b4 : 80419308561211 (30ms, Hashfull = 1000, ThreadId = 1)
//info string b2b4 : 80419308561211 (30ms, Hashfull = 1000, ThreadId = 3)
//info string b2b4 : 80419308561211 (28ms, Hashfull = 1000, ThreadId = 7)
//info string d2d4 : 211583204457112 (52ms, Hashfull = 1000, ThreadId = 0)
//info string d2d4 : 211583204457112 (53ms, Hashfull = 1000, ThreadId = 2)
//info string d2d4 : 211583204457112 (55ms, Hashfull = 1000, ThreadId = 4)
//info string a2a4 : 85054341127064 (31ms, Hashfull = 1000, ThreadId = 3)
//info string a2a4 : 85054341127064 (30ms, Hashfull = 1000, ThreadId = 7)
//info string b2b4 : 80419308561211 (61ms, Hashfull = 1000, ThreadId = 5)
//info string d2d4 : 211583204457112 (57ms, Hashfull = 1000, ThreadId = 6)
//info string a2a4 : 85054341127064 (57ms, Hashfull = 1000, ThreadId = 1)
//info string h2h3 : 60097879424719 (21ms, Hashfull = 1000, ThreadId = 7)
//info string a2a4 : 85054341127064 (32ms, Hashfull = 1000, ThreadId = 5)
//info string e2e4 : 245841494675197 (53ms, Hashfull = 1000, ThreadId = 0)
//info string h2h3 : 60097879424719 (19ms, Hashfull = 1000, ThreadId = 1)
//info string h2h3 : 60097879424719 (42ms, Hashfull = 1000, ThreadId = 3)
//info string e2e4 : 245841494675197 (63ms, Hashfull = 1000, ThreadId = 2)
//info string g2g3 : 82762826570051 (31ms, Hashfull = 1000, ThreadId = 7)
//info string e2e4 : 245841494675197 (53ms, Hashfull = 1000, ThreadId = 6)
//info string h2h3 : 60097879424719 (23ms, Hashfull = 1000, ThreadId = 5)
//info string f2f4 : 68372448303691 (23ms, Hashfull = 1000, ThreadId = 0)
//info string g2g3 : 82762826570051 (28ms, Hashfull = 1000, ThreadId = 3)
//info string e2e4 : 245841494675197 (77ms, Hashfull = 1000, ThreadId = 4)
//info string f2f3 : 51614296095395 (20ms, Hashfull = 1000, ThreadId = 7)
//info string f2f4 : 68372448303691 (23ms, Hashfull = 1000, ThreadId = 2)
//info string f2f4 : 68372448303691 (25ms, Hashfull = 1000, ThreadId = 6)
//info string g2g3 : 82762826570051 (31ms, Hashfull = 1000, ThreadId = 5)
//info string f2f3 : 51614296095395 (21ms, Hashfull = 1000, ThreadId = 3)
//info string g2g4 : 73966186324024 (29ms, Hashfull = 1000, ThreadId = 0)
//info string g2g4 : 73966186324024 (25ms, Hashfull = 1000, ThreadId = 2)
//info string f2f4 : 68372448303691 (27ms, Hashfull = 1000, ThreadId = 4)
//info string g2g4 : 73966186324024 (26ms, Hashfull = 1000, ThreadId = 6)
//info string f2f3 : 51614296095395 (21ms, Hashfull = 1000, ThreadId = 5)
//info string h2h4 : 86739921618220 (32ms, Hashfull = 1000, ThreadId = 0)
//info string h2h4 : 86739921618220 (24ms, Hashfull = 1000, ThreadId = 2)
//info string g2g4 : 73966186324024 (27ms, Hashfull = 1000, ThreadId = 4)
//info string h2h4 : 86739921618220 (24ms, Hashfull = 1000, ThreadId = 6)
//info string g2g3 : 82762826570051 (94ms, Hashfull = 1000, ThreadId = 1)
//info string e2e3 : 241074613621302 (63ms, Hashfull = 1000, ThreadId = 7)
//info string b1a3 : 70080800068168 (26ms, Hashfull = 1000, ThreadId = 2)
//info string b1a3 : 70080800068168 (22ms, Hashfull = 1000, ThreadId = 6)
//info string f2f3 : 51614296095395 (21ms, Hashfull = 1000, ThreadId = 1)
//info string e2e3 : 241074613621302 (65ms, Hashfull = 1000, ThreadId = 3)
//info string h2h4 : 86739921618220 (32ms, Hashfull = 1000, ThreadId = 4)
//info string b1a3 : 70080800068168 (43ms, Hashfull = 1000, ThreadId = 0)
//info string e2e3 : 241074613621302 (58ms, Hashfull = 1000, ThreadId = 5)
//info string b1c3 : 91451554526572 (32ms, Hashfull = 1000, ThreadId = 2)
//info string b1a3 : 70080800068168 (26ms, Hashfull = 1000, ThreadId = 4)
//info string b1c3 : 91451554526572 (34ms, Hashfull = 1000, ThreadId = 6)
//info string d2d3 : 151857971385067 (52ms, Hashfull = 1000, ThreadId = 7)
//info string b1c3 : 91451554526572 (35ms, Hashfull = 1000, ThreadId = 0)
//info string d2d3 : 151857971385067 (50ms, Hashfull = 1000, ThreadId = 3)
//info string g1f3 : 89933046388964 (29ms, Hashfull = 1000, ThreadId = 2)
//info string e2e3 : 241074613621302 (58ms, Hashfull = 1000, ThreadId = 1)
//info string d2d3 : 151857971385067 (46ms, Hashfull = 1000, ThreadId = 5)
//info string g1f3 : 89933046388964 (32ms, Hashfull = 1000, ThreadId = 6)
//info string c2c3 : 92235553734553 (30ms, Hashfull = 1000, ThreadId = 7)
//info string b1c3 : 91451554526572 (34ms, Hashfull = 1000, ThreadId = 4)
//info string g1f3 : 89933046388964 (34ms, Hashfull = 1000, ThreadId = 0)
//info string c2c3 : 92235553734553 (33ms, Hashfull = 1000, ThreadId = 3)
//info string c2c3 : 92235553734553 (30ms, Hashfull = 1000, ThreadId = 5)
//info string b2b3 : 79510326025357 (27ms, Hashfull = 1000, ThreadId = 7)
//info string g1f3 : 89933046388964 (32ms, Hashfull = 1000, ThreadId = 4)
//info string d2d3 : 151857971385067 (48ms, Hashfull = 1000, ThreadId = 1)
//info string a2a3 : 60403292887824 (20ms, Hashfull = 1000, ThreadId = 7)
//info string b2b3 : 79510326025357 (24ms, Hashfull = 1000, ThreadId = 5)
//info string a2a3 : 60403292887824 (18ms, Hashfull = 1000, ThreadId = 5)
//info string b2b3 : 79510326025357 (52ms, Hashfull = 1000, ThreadId = 3)
//info string c2c3 : 92235553734553 (29ms, Hashfull = 1000, ThreadId = 1)
//info string a2a3 : 60403292887824 (21ms, Hashfull = 1000, ThreadId = 3)
//info string b2b3 : 79510326025357 (25ms, Hashfull = 1000, ThreadId = 1)
//info string a2a3 : 60403292887824 (20ms, Hashfull = 1000, ThreadId = 1)
//info string g1h3 : 71046267678634 (9364ms, Hashfull = 1000, ThreadId = 0)
//info string Total : 2097651003696806 (15845486ms, 132381613520 leaves / s)
//info string Perft stores = 4889677061
//info string Perft stores successful = 853918014
//info string Perft probes = 5595210186
//info string Perft probes successful = 704604750
//info string Perft positions actually searched = 3957524446577 (0.19%)
//info string Perft positions from transposition table = 2093693479250229 (99.81%)


// 0.9 hours {54.7 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//debug on
//info string Debug set to true
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string Transposition table memory = 16384MB(17179869184 bytes)
//info string Perft transposition table bucket size = 64 bytes
//info string Perft transposition table entry size = 16 bytes
//info string Perft transposition table entries per bucket = 4
//info string Perft transposition table buckets = 268435456
//info string Perft transposition table entries = 1073741824
//info string Perft transposition table memory allocated = 16384MB(17179869184 bytes)
//info string g2g4 : 73966186324024 (1687246ms, Hashfull = 1000, ThreadId = 5)
//info string g2g4 : 73966186324024 (1687248ms, Hashfull = 1000, ThreadId = 14)
//info string a2a4 : 85054341127064 (1859406ms, Hashfull = 1000, ThreadId = 11)
//info string a2a4 : 85054341127064 (1859407ms, Hashfull = 1000, ThreadId = 8)
//info string g2g3 : 82762826570051 (1863727ms, Hashfull = 1000, ThreadId = 13)
//info string g2g3 : 82762826570051 (1863728ms, Hashfull = 1000, ThreadId = 6)
//info string c2c4 : 103605670223681 (1963501ms, Hashfull = 1000, ThreadId = 10)
//info string c2c4 : 103605670223681 (1963504ms, Hashfull = 1000, ThreadId = 9)
//info string a2a3 : 60403292887824 (2577423ms, Hashfull = 1000, ThreadId = 0)
//info string h2h3 : 60097879424719 (762670ms, Hashfull = 1000, ThreadId = 6)
//info string h2h3 : 60097879424719 (766992ms, Hashfull = 1000, ThreadId = 11)
//info string a2a4 : 85054341127064 (21ms, Hashfull = 1000, ThreadId = 6)
//info string g2g3 : 82762826570051 (26ms, Hashfull = 1000, ThreadId = 11)
//info string b2b4 : 80419308561211 (105119ms, Hashfull = 1000, ThreadId = 6)
//info string b2b4 : 80419308561211 (872132ms, Hashfull = 1000, ThreadId = 8)
//info string b2b4 : 80419308561211 (768035ms, Hashfull = 1000, ThreadId = 9)
//info string c2c4 : 103605670223681 (24ms, Hashfull = 1000, ThreadId = 6)
//info string c2c4 : 103605670223681 (24ms, Hashfull = 1000, ThreadId = 8)
//info string a2a4 : 85054341127064 (26ms, Hashfull = 1000, ThreadId = 9)
//info string h2h3 : 60097879424719 (17ms, Hashfull = 1000, ThreadId = 9)
//info string g2g3 : 82762826570051 (22ms, Hashfull = 1000, ThreadId = 9)
//info string b1a3 : 70080800068168 (2756020ms, Hashfull = 1000, ThreadId = 3)
//info string f2f3 : 51614296095395 (50437ms, Hashfull = 1000, ThreadId = 9)
//info string f2f3 : 51614296095395 (155619ms, Hashfull = 1000, ThreadId = 11)
//info string f2f3 : 51614296095395 (918316ms, Hashfull = 1000, ThreadId = 13)
//info string g1f3 : 89933046388964 (2817696ms, Hashfull = 1000, ThreadId = 1)
//info string c2c3 : 92235553734553 (2873006ms, Hashfull = 1000, ThreadId = 2)
//info string f2f4 : 68372448303691 (1236693ms, Hashfull = 1000, ThreadId = 5)
//info string h2h4 : 86739921618220 (1302660ms, Hashfull = 1000, ThreadId = 14)
//info string h2h4 : 86739921618220 (233897ms, Hashfull = 1000, ThreadId = 3)
//info string b1a3 : 70080800068168 (19ms, Hashfull = 1000, ThreadId = 14)
//info string g2g4 : 73966186324024 (22ms, Hashfull = 1000, ThreadId = 3)
//info string f2f4 : 68372448303691 (19ms, Hashfull = 1000, ThreadId = 3)
//info string e2e3 : 241074613621302 (317076ms, Hashfull = 1000, ThreadId = 9)
//info string e2e3 : 241074613621302 (317079ms, Hashfull = 1000, ThreadId = 11)
//info string e2e3 : 241074613621302 (317081ms, Hashfull = 1000, ThreadId = 13)
//info string e2e3 : 241074613621302 (3099128ms, Hashfull = 1000, ThreadId = 15)
//info string e2e3 : 241074613621302 (3099136ms, Hashfull = 1000, ThreadId = 4)
//info string f2f3 : 51614296095395 (16ms, Hashfull = 1000, ThreadId = 4)
//info string g2g3 : 82762826570051 (25ms, Hashfull = 1000, ThreadId = 4)
//info string h2h3 : 60097879424719 (15ms, Hashfull = 1000, ThreadId = 4)
//info string a2a4 : 85054341127064 (25ms, Hashfull = 1000, ThreadId = 4)
//info string b2b4 : 80419308561211 (20ms, Hashfull = 1000, ThreadId = 4)
//info string c2c4 : 103605670223681 (30ms, Hashfull = 1000, ThreadId = 4)
//info string b1c3 : 91451554526572 (352424ms, Hashfull = 1000, ThreadId = 1)
//info string b1c3 : 91451554526572 (180195ms, Hashfull = 1000, ThreadId = 14)
//info string b1a3 : 70080800068168 (21ms, Hashfull = 1000, ThreadId = 1)
//info string g1f3 : 89933046388964 (28ms, Hashfull = 1000, ThreadId = 14)
//info string h2h4 : 86739921618220 (27ms, Hashfull = 1000, ThreadId = 1)
//info string g2g4 : 73966186324024 (23ms, Hashfull = 1000, ThreadId = 1)
//info string f2f4 : 68372448303691 (20ms, Hashfull = 1000, ThreadId = 1)
//info string e2e4 : 245841494675197 (10691ms, Hashfull = 1000, ThreadId = 1)
//info string e2e4 : 245841494675197 (190952ms, Hashfull = 1000, ThreadId = 3)
//info string e2e4 : 245841494675197 (256976ms, Hashfull = 1000, ThreadId = 5)
//info string e2e4 : 245841494675197 (3180914ms, Hashfull = 1000, ThreadId = 12)
//info string e2e4 : 245841494675197 (3180917ms, Hashfull = 1000, ThreadId = 7)
//info string f2f4 : 68372448303691 (18ms, Hashfull = 1000, ThreadId = 12)
//info string g2g4 : 73966186324024 (24ms, Hashfull = 1000, ThreadId = 12)
//info string h2h4 : 86739921618220 (25ms, Hashfull = 1000, ThreadId = 12)
//info string b1a3 : 70080800068168 (20ms, Hashfull = 1000, ThreadId = 12)
//info string b1c3 : 91451554526572 (25ms, Hashfull = 1000, ThreadId = 12)
//info string g1f3 : 89933046388964 (24ms, Hashfull = 1000, ThreadId = 12)
//info string g1h3 : 71046267678634 (61297ms, Hashfull = 1000, ThreadId = 12)
//info string g1h3 : 71046267678634 (72200ms, Hashfull = 1000, ThreadId = 14)
//info string a2a3 : 60403292887824 (16ms, Hashfull = 1000, ThreadId = 12)
//info string a2a3 : 60403292887824 (38ms, Hashfull = 1000, ThreadId = 14)
//info string b2b3 : 79510326025357 (665949ms, Hashfull = 1000, ThreadId = 0)
//info string b2b3 : 79510326025357 (1005ms, Hashfull = 1000, ThreadId = 14)
//info string b2b3 : 79510326025357 (1031ms, Hashfull = 1000, ThreadId = 12)
//info string c2c3 : 92235553734553 (24ms, Hashfull = 1000, ThreadId = 0)
//info string c2c3 : 92235553734553 (15ms, Hashfull = 1000, ThreadId = 12)
//info string c2c3 : 92235553734553 (18ms, Hashfull = 1000, ThreadId = 14)
//info string d2d4 : 211583204457112 (91532ms, Hashfull = 1000, ThreadId = 5)
//info string d2d4 : 211583204457112 (91545ms, Hashfull = 1000, ThreadId = 3)
//info string d2d4 : 211583204457112 (91553ms, Hashfull = 1000, ThreadId = 1)
//info string d2d4 : 211583204457112 (540895ms, Hashfull = 1000, ThreadId = 6)
//info string d2d4 : 211583204457112 (173195ms, Hashfull = 1000, ThreadId = 4)
//info string d2d4 : 211583204457112 (91544ms, Hashfull = 1000, ThreadId = 7)
//info string d2d4 : 211583204457112 (540898ms, Hashfull = 1000, ThreadId = 8)
//info string d2d4 : 211583204457112 (1308970ms, Hashfull = 1000, ThreadId = 10)
//info string c2c4 : 103605670223681 (23ms, Hashfull = 1000, ThreadId = 3)
//info string c2c4 : 103605670223681 (31ms, Hashfull = 1000, ThreadId = 5)
//info string c2c4 : 103605670223681 (27ms, Hashfull = 1000, ThreadId = 1)
//info string c2c4 : 103605670223681 (28ms, Hashfull = 1000, ThreadId = 7)
//info string b2b4 : 80419308561211 (18ms, Hashfull = 1000, ThreadId = 5)
//info string b2b4 : 80419308561211 (17ms, Hashfull = 1000, ThreadId = 1)
//info string e2e4 : 245841494675197 (46ms, Hashfull = 1000, ThreadId = 6)
//info string b2b4 : 80419308561211 (19ms, Hashfull = 1000, ThreadId = 7)
//info string e2e4 : 245841494675197 (46ms, Hashfull = 1000, ThreadId = 8)
//info string e2e4 : 245841494675197 (48ms, Hashfull = 1000, ThreadId = 4)
//info string a2a4 : 85054341127064 (17ms, Hashfull = 1000, ThreadId = 1)
//info string b2b4 : 80419308561211 (37ms, Hashfull = 1000, ThreadId = 3)
//info string e2e4 : 245841494675197 (44ms, Hashfull = 1000, ThreadId = 10)
//info string a2a4 : 85054341127064 (22ms, Hashfull = 1000, ThreadId = 5)
//info string f2f4 : 68372448303691 (14ms, Hashfull = 1000, ThreadId = 8)
//info string f2f4 : 68372448303691 (14ms, Hashfull = 1000, ThreadId = 4)
//info string f2f4 : 68372448303691 (11ms, Hashfull = 1000, ThreadId = 10)
//info string f2f4 : 68372448303691 (26ms, Hashfull = 1000, ThreadId = 6)
//info string a2a4 : 85054341127064 (22ms, Hashfull = 1000, ThreadId = 7)
//info string a2a4 : 85054341127064 (19ms, Hashfull = 1000, ThreadId = 3)
//info string h2h3 : 60097879424719 (19ms, Hashfull = 1000, ThreadId = 5)
//info string h2h3 : 60097879424719 (24ms, Hashfull = 1000, ThreadId = 1)
//info string h2h3 : 60097879424719 (10ms, Hashfull = 1000, ThreadId = 3)
//info string h2h3 : 60097879424719 (14ms, Hashfull = 1000, ThreadId = 7)
//info string g2g4 : 73966186324024 (25ms, Hashfull = 1000, ThreadId = 4)
//info string g2g4 : 73966186324024 (24ms, Hashfull = 1000, ThreadId = 10)
//info string g2g4 : 73966186324024 (25ms, Hashfull = 1000, ThreadId = 6)
//info string g2g3 : 82762826570051 (20ms, Hashfull = 1000, ThreadId = 5)
//info string g2g3 : 82762826570051 (13ms, Hashfull = 1000, ThreadId = 7)
//info string g2g4 : 73966186324024 (37ms, Hashfull = 1000, ThreadId = 8)
//info string g2g3 : 82762826570051 (19ms, Hashfull = 1000, ThreadId = 3)
//info string f2f3 : 51614296095395 (8ms, Hashfull = 1000, ThreadId = 3)
//info string f2f3 : 51614296095395 (13ms, Hashfull = 1000, ThreadId = 7)
//info string g2g3 : 82762826570051 (33ms, Hashfull = 1000, ThreadId = 1)
//info string h2h4 : 86739921618220 (28ms, Hashfull = 1000, ThreadId = 4)
//info string h2h4 : 86739921618220 (28ms, Hashfull = 1000, ThreadId = 10)
//info string f2f3 : 51614296095395 (9ms, Hashfull = 1000, ThreadId = 1)
//info string h2h4 : 86739921618220 (27ms, Hashfull = 1000, ThreadId = 6)
//info string f2f3 : 51614296095395 (25ms, Hashfull = 1000, ThreadId = 5)
//info string h2h4 : 86739921618220 (23ms, Hashfull = 1000, ThreadId = 8)
//info string b1a3 : 70080800068168 (24ms, Hashfull = 1000, ThreadId = 4)
//info string b1a3 : 70080800068168 (24ms, Hashfull = 1000, ThreadId = 10)
//info string b1a3 : 70080800068168 (25ms, Hashfull = 1000, ThreadId = 8)
//info string e2e3 : 241074613621302 (39ms, Hashfull = 1000, ThreadId = 1)
//info string e2e3 : 241074613621302 (49ms, Hashfull = 1000, ThreadId = 3)
//info string e2e3 : 241074613621302 (42ms, Hashfull = 1000, ThreadId = 5)
//info string b1a3 : 70080800068168 (48ms, Hashfull = 1000, ThreadId = 6)
//info string b1c3 : 91451554526572 (29ms, Hashfull = 1000, ThreadId = 4)
//info string b1c3 : 91451554526572 (30ms, Hashfull = 1000, ThreadId = 10)
//info string e2e3 : 241074613621302 (66ms, Hashfull = 1000, ThreadId = 7)
//info string g1f3 : 89933046388964 (26ms, Hashfull = 1000, ThreadId = 10)
//info string b1c3 : 91451554526572 (31ms, Hashfull = 1000, ThreadId = 6)
//info string g1f3 : 89933046388964 (32ms, Hashfull = 1000, ThreadId = 4)
//info string g1h3 : 71046267678634 (15ms, Hashfull = 1000, ThreadId = 4)
//info string g1h3 : 71046267678634 (19ms, Hashfull = 1000, ThreadId = 10)
//info string g1f3 : 89933046388964 (28ms, Hashfull = 1000, ThreadId = 6)
//info string a2a3 : 60403292887824 (21ms, Hashfull = 1000, ThreadId = 4)
//info string a2a3 : 60403292887824 (21ms, Hashfull = 1000, ThreadId = 10)
//info string g1h3 : 71046267678634 (20ms, Hashfull = 1000, ThreadId = 6)
//info string b2b3 : 79510326025357 (17ms, Hashfull = 1000, ThreadId = 4)
//info string b2b3 : 79510326025357 (21ms, Hashfull = 1000, ThreadId = 10)
//info string a2a3 : 60403292887824 (19ms, Hashfull = 1000, ThreadId = 6)
//info string c2c3 : 92235553734553 (20ms, Hashfull = 1000, ThreadId = 4)
//info string c2c3 : 92235553734553 (22ms, Hashfull = 1000, ThreadId = 10)
//info string b2b3 : 79510326025357 (24ms, Hashfull = 1000, ThreadId = 6)
//info string c2c3 : 92235553734553 (26ms, Hashfull = 1000, ThreadId = 6)
//info string b1c3 : 91451554526572 (207ms, Hashfull = 1000, ThreadId = 8)
//info string g1f3 : 89933046388964 (28ms, Hashfull = 1000, ThreadId = 8)
//info string g1h3 : 71046267678634 (20ms, Hashfull = 1000, ThreadId = 8)
//info string a2a3 : 60403292887824 (18ms, Hashfull = 1000, ThreadId = 8)
//info string b2b3 : 79510326025357 (27ms, Hashfull = 1000, ThreadId = 8)
//info string c2c3 : 92235553734553 (26ms, Hashfull = 1000, ThreadId = 8)
//info string d2d3 : 151857971385067 (36890ms, Hashfull = 1000, ThreadId = 12)
//info string d2d3 : 151857971385067 (181178ms, Hashfull = 1000, ThreadId = 11)
//info string d2d3 : 151857971385067 (36892ms, Hashfull = 1000, ThreadId = 14)
//info string d2d3 : 151857971385067 (181188ms, Hashfull = 1000, ThreadId = 9)
//info string d2d3 : 151857971385067 (36911ms, Hashfull = 1000, ThreadId = 0)
//info string d2d3 : 151857971385067 (181181ms, Hashfull = 1000, ThreadId = 15)
//info string d2d3 : 151857971385067 (181186ms, Hashfull = 1000, ThreadId = 13)
//info string d2d3 : 151857971385067 (7706ms, Hashfull = 1000, ThreadId = 1)
//info string d2d3 : 151857971385067 (7398ms, Hashfull = 1000, ThreadId = 8)
//info string d2d3 : 151857971385067 (7556ms, Hashfull = 1000, ThreadId = 6)
//info string d2d3 : 151857971385067 (7708ms, Hashfull = 1000, ThreadId = 5)
//info string c2c3 : 92235553734553 (25ms, Hashfull = 1000, ThreadId = 9)
//info string d2d3 : 151857971385067 (407323ms, Hashfull = 1000, ThreadId = 2)
//info string c2c3 : 92235553734553 (21ms, Hashfull = 1000, ThreadId = 13)
//info string d2d3 : 151857971385067 (7603ms, Hashfull = 1000, ThreadId = 4)
//info string d2d3 : 151857971385067 (7700ms, Hashfull = 1000, ThreadId = 7)
//info string c2c3 : 92235553734553 (35ms, Hashfull = 1000, ThreadId = 11)
//info string d2d3 : 151857971385067 (7602ms, Hashfull = 1000, ThreadId = 10)
//info string d2d3 : 151857971385067 (7726ms, Hashfull = 1000, ThreadId = 3)
//info string c2c3 : 92235553734553 (38ms, Hashfull = 1000, ThreadId = 15)
//info string b2b3 : 79510326025357 (22ms, Hashfull = 1000, ThreadId = 9)
//info string c2c3 : 92235553734553 (24ms, Hashfull = 1000, ThreadId = 5)
//info string e2e3 : 241074613621302 (55ms, Hashfull = 1000, ThreadId = 14)
//info string b2b3 : 79510326025357 (24ms, Hashfull = 1000, ThreadId = 13)
//info string e2e3 : 241074613621302 (51ms, Hashfull = 1000, ThreadId = 0)
//info string c2c3 : 92235553734553 (27ms, Hashfull = 1000, ThreadId = 7)
//info string b2b3 : 79510326025357 (27ms, Hashfull = 1000, ThreadId = 11)
//info string e2e3 : 241074613621302 (67ms, Hashfull = 1000, ThreadId = 12)
//info string a2a3 : 60403292887824 (17ms, Hashfull = 1000, ThreadId = 9)
//info string c2c3 : 92235553734553 (26ms, Hashfull = 1000, ThreadId = 3)
//info string b2b3 : 79510326025357 (21ms, Hashfull = 1000, ThreadId = 15)
//info string f2f3 : 51614296095395 (15ms, Hashfull = 1000, ThreadId = 14)
//info string e2e3 : 241074613621302 (48ms, Hashfull = 1000, ThreadId = 6)
//info string e2e3 : 241074613621302 (52ms, Hashfull = 1000, ThreadId = 8)
//info string a2a3 : 60403292887824 (18ms, Hashfull = 1000, ThreadId = 11)
//info string f2f3 : 51614296095395 (16ms, Hashfull = 1000, ThreadId = 12)
//info string e2e3 : 241074613621302 (49ms, Hashfull = 1000, ThreadId = 2)
//info string a2a3 : 60403292887824 (27ms, Hashfull = 1000, ThreadId = 13)
//info string b2b3 : 79510326025357 (24ms, Hashfull = 1000, ThreadId = 7)
//info string a2a3 : 60403292887824 (16ms, Hashfull = 1000, ThreadId = 15)
//info string f2f3 : 51614296095395 (28ms, Hashfull = 1000, ThreadId = 0)
//info string e2e3 : 241074613621302 (48ms, Hashfull = 1000, ThreadId = 10)
//info string f2f3 : 51614296095395 (13ms, Hashfull = 1000, ThreadId = 6)
//info string g2g3 : 82762826570051 (22ms, Hashfull = 1000, ThreadId = 14)
//info string b2b3 : 79510326025357 (40ms, Hashfull = 1000, ThreadId = 5)
//info string g1h3 : 71046267678634 (27ms, Hashfull = 1000, ThreadId = 9)
//info string f2f3 : 51614296095395 (15ms, Hashfull = 1000, ThreadId = 2)
//info string b2b3 : 79510326025357 (28ms, Hashfull = 1000, ThreadId = 3)
//info string g1h3 : 71046267678634 (21ms, Hashfull = 1000, ThreadId = 11)
//info string g1h3 : 71046267678634 (18ms, Hashfull = 1000, ThreadId = 15)
//info string g2g3 : 82762826570051 (24ms, Hashfull = 1000, ThreadId = 12)
//info string a2a3 : 60403292887824 (21ms, Hashfull = 1000, ThreadId = 7)
//info string f2f3 : 51614296095395 (30ms, Hashfull = 1000, ThreadId = 8)
//info string f2f3 : 51614296095395 (20ms, Hashfull = 1000, ThreadId = 10)
//info string h2h3 : 60097879424719 (17ms, Hashfull = 1000, ThreadId = 14)
//info string a2a3 : 60403292887824 (16ms, Hashfull = 1000, ThreadId = 5)
//info string g1h3 : 71046267678634 (26ms, Hashfull = 1000, ThreadId = 13)
//info string a2a3 : 60403292887824 (13ms, Hashfull = 1000, ThreadId = 3)
//info string g2g3 : 82762826570051 (23ms, Hashfull = 1000, ThreadId = 2)
//info string h2h3 : 60097879424719 (17ms, Hashfull = 1000, ThreadId = 12)
//info string g1h3 : 71046267678634 (16ms, Hashfull = 1000, ThreadId = 7)
//info string g2g3 : 82762826570051 (36ms, Hashfull = 1000, ThreadId = 0)
//info string g1f3 : 89933046388964 (25ms, Hashfull = 1000, ThreadId = 11)
//info string g1f3 : 89933046388964 (32ms, Hashfull = 1000, ThreadId = 9)
//info string g1f3 : 89933046388964 (27ms, Hashfull = 1000, ThreadId = 15)
//info string g1h3 : 71046267678634 (23ms, Hashfull = 1000, ThreadId = 5)
//info string g1f3 : 89933046388964 (23ms, Hashfull = 1000, ThreadId = 13)
//info string g2g3 : 82762826570051 (26ms, Hashfull = 1000, ThreadId = 10)
//info string a2a4 : 85054341127064 (26ms, Hashfull = 1000, ThreadId = 14)
//info string g2g3 : 82762826570051 (27ms, Hashfull = 1000, ThreadId = 8)
//info string g1h3 : 71046267678634 (26ms, Hashfull = 1000, ThreadId = 3)
//info string h2h3 : 60097879424719 (14ms, Hashfull = 1000, ThreadId = 0)
//info string h2h3 : 60097879424719 (20ms, Hashfull = 1000, ThreadId = 2)
//info string c2c3 : 92235553734553 (120ms, Hashfull = 1000, ThreadId = 1)
//info string b1c3 : 91451554526572 (17ms, Hashfull = 1000, ThreadId = 9)
//info string a2a4 : 85054341127064 (25ms, Hashfull = 1000, ThreadId = 12)
//info string b1c3 : 91451554526572 (21ms, Hashfull = 1000, ThreadId = 11)
//info string b1c3 : 91451554526572 (15ms, Hashfull = 1000, ThreadId = 13)
//info string b1c3 : 91451554526572 (20ms, Hashfull = 1000, ThreadId = 15)
//info string h2h3 : 60097879424719 (20ms, Hashfull = 1000, ThreadId = 10)
//info string h2h3 : 60097879424719 (20ms, Hashfull = 1000, ThreadId = 8)
//info string a2a4 : 85054341127064 (22ms, Hashfull = 1000, ThreadId = 2)
//info string b2b4 : 80419308561211 (25ms, Hashfull = 1000, ThreadId = 14)
//info string g1f3 : 89933046388964 (31ms, Hashfull = 1000, ThreadId = 5)
//info string g1f3 : 89933046388964 (28ms, Hashfull = 1000, ThreadId = 3)
//info string b2b3 : 79510326025357 (23ms, Hashfull = 1000, ThreadId = 1)
//info string g1f3 : 89933046388964 (45ms, Hashfull = 1000, ThreadId = 7)
//info string b1a3 : 70080800068168 (22ms, Hashfull = 1000, ThreadId = 13)
//info string b2b4 : 80419308561211 (25ms, Hashfull = 1000, ThreadId = 12)
//info string b1a3 : 70080800068168 (21ms, Hashfull = 1000, ThreadId = 15)
//info string a2a4 : 85054341127064 (24ms, Hashfull = 1000, ThreadId = 10)
//info string b1a3 : 70080800068168 (35ms, Hashfull = 1000, ThreadId = 9)
//info string a2a4 : 85054341127064 (43ms, Hashfull = 1000, ThreadId = 0)
//info string b2b4 : 80419308561211 (25ms, Hashfull = 1000, ThreadId = 2)
//info string a2a3 : 60403292887824 (20ms, Hashfull = 1000, ThreadId = 1)
//info string c2c4 : 103605670223681 (30ms, Hashfull = 1000, ThreadId = 14)
//info string b1c3 : 91451554526572 (26ms, Hashfull = 1000, ThreadId = 3)
//info string b1a3 : 70080800068168 (45ms, Hashfull = 1000, ThreadId = 11)
//info string b1c3 : 91451554526572 (29ms, Hashfull = 1000, ThreadId = 5)
//info string b1c3 : 91451554526572 (26ms, Hashfull = 1000, ThreadId = 7)
//info string b2b4 : 80419308561211 (19ms, Hashfull = 1000, ThreadId = 10)
//info string h2h4 : 86739921618220 (27ms, Hashfull = 1000, ThreadId = 15)
//info string c2c4 : 103605670223681 (29ms, Hashfull = 1000, ThreadId = 12)
//info string b2b4 : 80419308561211 (20ms, Hashfull = 1000, ThreadId = 0)
//info string h2h4 : 86739921618220 (39ms, Hashfull = 1000, ThreadId = 13)
//info string h2h4 : 86739921618220 (30ms, Hashfull = 1000, ThreadId = 9)
//info string g1h3 : 71046267678634 (24ms, Hashfull = 1000, ThreadId = 1)
//info string b1a3 : 70080800068168 (24ms, Hashfull = 1000, ThreadId = 5)
//info string h2h4 : 86739921618220 (27ms, Hashfull = 1000, ThreadId = 11)
//info string c2c4 : 103605670223681 (35ms, Hashfull = 1000, ThreadId = 2)
//info string b1a3 : 70080800068168 (26ms, Hashfull = 1000, ThreadId = 7)
//info string g2g4 : 73966186324024 (15ms, Hashfull = 1000, ThreadId = 9)
//info string g2g4 : 73966186324024 (27ms, Hashfull = 1000, ThreadId = 15)
//info string g2g4 : 73966186324024 (19ms, Hashfull = 1000, ThreadId = 13)
//info string c2c4 : 103605670223681 (29ms, Hashfull = 1000, ThreadId = 0)
//info string g2g4 : 73966186324024 (13ms, Hashfull = 1000, ThreadId = 11)
//info string d2d4 : 211583204457112 (43ms, Hashfull = 1000, ThreadId = 14)
//info string d2d4 : 211583204457112 (38ms, Hashfull = 1000, ThreadId = 12)
//info string h2h4 : 86739921618220 (26ms, Hashfull = 1000, ThreadId = 5)
//info string h2h4 : 86739921618220 (24ms, Hashfull = 1000, ThreadId = 7)
//info string f2f4 : 68372448303691 (17ms, Hashfull = 1000, ThreadId = 11)
//info string f2f4 : 68372448303691 (25ms, Hashfull = 1000, ThreadId = 15)
//info string f2f4 : 68372448303691 (25ms, Hashfull = 1000, ThreadId = 13)
//info string d2d4 : 211583204457112 (37ms, Hashfull = 1000, ThreadId = 2)
//info string d2d4 : 211583204457112 (35ms, Hashfull = 1000, ThreadId = 0)
//info string g2g4 : 73966186324024 (24ms, Hashfull = 1000, ThreadId = 7)
//info string f2f4 : 68372448303691 (49ms, Hashfull = 1000, ThreadId = 9)
//info string e2e4 : 245841494675197 (43ms, Hashfull = 1000, ThreadId = 14)
//info string e2e4 : 245841494675197 (35ms, Hashfull = 1000, ThreadId = 2)
//info string e2e4 : 245841494675197 (47ms, Hashfull = 1000, ThreadId = 11)
//info string f2f4 : 68372448303691 (29ms, Hashfull = 1000, ThreadId = 7)
//info string f2f4 : 68372448303691 (22ms, Hashfull = 1000, ThreadId = 14)
//info string e2e4 : 245841494675197 (47ms, Hashfull = 1000, ThreadId = 13)
//info string e2e4 : 245841494675197 (39ms, Hashfull = 1000, ThreadId = 0)
//info string e2e4 : 245841494675197 (52ms, Hashfull = 1000, ThreadId = 15)
//info string f2f4 : 68372448303691 (16ms, Hashfull = 1000, ThreadId = 2)
//info string f2f4 : 68372448303691 (11ms, Hashfull = 1000, ThreadId = 0)
//info string e2e4 : 245841494675197 (39ms, Hashfull = 1000, ThreadId = 9)
//info string g2g4 : 73966186324024 (14ms, Hashfull = 1000, ThreadId = 0)
//info string g2g4 : 73966186324024 (21ms, Hashfull = 1000, ThreadId = 2)
//info string d2d4 : 211583204457112 (45ms, Hashfull = 1000, ThreadId = 11)
//info string d2d4 : 211583204457112 (43ms, Hashfull = 1000, ThreadId = 13)
//info string d2d4 : 211583204457112 (42ms, Hashfull = 1000, ThreadId = 15)
//info string h2h4 : 86739921618220 (22ms, Hashfull = 1000, ThreadId = 0)
//info string d2d4 : 211583204457112 (35ms, Hashfull = 1000, ThreadId = 9)
//info string c2c4 : 103605670223681 (24ms, Hashfull = 1000, ThreadId = 11)
//info string h2h4 : 86739921618220 (36ms, Hashfull = 1000, ThreadId = 2)
//info string b1a3 : 70080800068168 (19ms, Hashfull = 1000, ThreadId = 0)
//info string c2c4 : 103605670223681 (23ms, Hashfull = 1000, ThreadId = 15)
//info string c2c4 : 103605670223681 (35ms, Hashfull = 1000, ThreadId = 13)
//info string b1a3 : 70080800068168 (18ms, Hashfull = 1000, ThreadId = 2)
//info string b2b4 : 80419308561211 (19ms, Hashfull = 1000, ThreadId = 15)
//info string b1c3 : 91451554526572 (24ms, Hashfull = 1000, ThreadId = 0)
//info string b2b4 : 80419308561211 (28ms, Hashfull = 1000, ThreadId = 11)
//info string b2b4 : 80419308561211 (16ms, Hashfull = 1000, ThreadId = 13)
//info string b1c3 : 91451554526572 (23ms, Hashfull = 1000, ThreadId = 2)
//info string a2a4 : 85054341127064 (15ms, Hashfull = 1000, ThreadId = 13)
//info string a2a4 : 85054341127064 (21ms, Hashfull = 1000, ThreadId = 15)
//info string g1f3 : 89933046388964 (26ms, Hashfull = 1000, ThreadId = 0)
//info string h2h3 : 60097879424719 (15ms, Hashfull = 1000, ThreadId = 13)
//info string g1f3 : 89933046388964 (29ms, Hashfull = 1000, ThreadId = 2)
//info string h2h3 : 60097879424719 (27ms, Hashfull = 1000, ThreadId = 15)
//info string g1h3 : 71046267678634 (19ms, Hashfull = 1000, ThreadId = 0)
//info string Total : 2097651003696806 (3280726ms, 639386222347 leaves / s)
//info string Perft stores = 1425456851
//info string Perft stores successful = 939606863
//info string Perft probes = 2215010321
//info string Perft probes successful = 789144267
//info string Perft positions actually searched = 1113864106382 (0.05%)
//info string Perft positions from transposition table = 2096537139590424 (99.95%)


// 0.9 hours {53.5 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (3212571ms, 652950862003 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1379070132
//info string Transposition table stores successful = 908887747 (65.91%)
//info string Transposition table probes = 2152508887
//info string Transposition table probes successful = 773261707 (35.92%)
//info string Positions counted in search = 1057576119410 (0.05%)
//info string Positions counted from transposition table = 2096593427577396 (99.95%)


// 0.89 hours {53.3 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (3197357ms, 656057801395 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1303330901
//info string Transposition table stores successful = 876288456 (67.23%)
//info string Transposition table probes = 2101795614
//info string Transposition table probes successful = 798287665 (37.98%)
//info string Positions counted in search = 1042903687433 (0.05%)
//info string Positions counted from transposition table = 2096608100009373 (99.95%)


// 0.73 hours {43.9 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (2634951ms, 796087291071 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1239790541
//info string Transposition table stores successful = 833610084 (67.24%)
//info string Transposition table stores failed due to read or write lock = 56270 (0.00%)
//info string Transposition table probes = 2004936471
//info string Transposition table probes successful = 764968882 (38.15%)
//info string Transposition table probes failed due to write lock = 3201 (0.00%)
//info string Positions counted in search = 988921731443 (0.05%)
//info string Positions counted from transposition table = 2096662081965363 (99.95%)


// 0.70 hours {41.9 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (2515482ms, 833896248789 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1325959057
//info string Transposition table stores successful = 891970306 (67.27%)
//info string Transposition table stores failed due to read or write lock = 85407 (0.01%)
//info string Transposition table probes = 2147809915
//info string Transposition table probes successful = 821673810 (38.26%)
//info string Transposition table probes failed due to write lock = 1740 (0.00%)
//info string Positions counted in search = 1061751933645 (0.05%)
//info string Positions counted from transposition table = 2096589251763161 (99.95%)


//go perft 11 NOT QUITE FULLY DEVELOPED LOCKLESS VERSION
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (2555352ms, 820885343270 leaves / s)
//info string Transposition table hashfull = 959
//info string Transposition table stores = 1313430421
//info string Transposition table stores successful = 882480689 (67.19%)
//info string Transposition table stores failed due to read or write lock = 0 (0.00%)
//info string Transposition table probes = 2118875234
//info string Transposition table probes successful = 805444355 (38.01%)
//info string Transposition table probes failed due to write lock = 0 (0.00%)
//info string Positions counted in search = 1051115519442 (0.05%)
//info string Positions counted from transposition table = 2096599888177364 (99.95%)


// 0.67 hours {40.1 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (2407972ms, 871127655843 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1350515424
//info string Transposition table stores successful = 907170542 (67.17%)
//info string Transposition table probes = 2183735109
//info string Transposition table probes successful = 833219227 (38.16%)
//info string Positions counted in search = 1081466526792 (0.05%)
//info string Positions counted from transposition table = 2096569537170014 (99.95%)


// 19.3 hours (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 12
//info string a2a3 : 1825396176881632
//info string b2b3 : 2407514849528875
//info string c2c3 : 2751675948507059
//info string d2d3 : 4588998634450632
//info string e2e3 : 7160631171539800
//info string f2f3 : 1552858858446419
//info string g2g3 : 2498600008341437
//info string h2h3 : 1814268178532771
//info string a2a4 : 2572564331526038
//info string b2b4 : 2412357918298534
//info string c2c4 : 3119892147087203
//info string d2d4 : 6326899070222383
//info string e2e4 : 7263638936690183
//info string f2f4 : 2050768802609121
//info string g2g4 : 2217762743088597
//info string h2h4 : 2620620274642577
//info string b1a3 : 2101612201748156
//info string b1c3 : 2731501636365779
//info string g1f3 : 2704348041301604
//info string g1h3 : 2133059306892947
//info string Total : 62854969236701747 (69518025ms, 108097677049 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 36627711657
//info string Transposition table stores successful = 1868500277 (5.10%)
//info string Transposition table probes = 38315534574
//info string Transposition table probes successful = 1687809801 (4.41%)
//info string Positions counted in search = 31764216725168 (0.05%)
//info string Positions counted from transposition table = 62823205019976579 (99.95%)


//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 12
//info string a2a3 : 1825396176881632
//info string b2b3 : 2407514849528875
//info string c2c3 : 2751675948507059
//info string d2d3 : 4588998634450632
//info string e2e3 : 7160631171539800
//info string f2f3 : 1552858858446419
//info string g2g3 : 2498600008341437
//info string h2h3 : 1814268178532771
//info string a2a4 : 2572564331526038
//info string b2b4 : 2412357918298534
//info string c2c4 : 3119892147087203
//info string d2d4 : 6326899070222383
//info string e2e4 : 7263638936690183
//info string f2f4 : 2050768802609121
//info string g2g4 : 2217762743088597
//info string h2h4 : 2620620274642577
//info string b1a3 : 2101612201748156
//info string b1c3 : 2731501636365779
//info string g1f3 : 2704348041301604
//info string g1h3 : 2133059306892947
//info string Total : 62854969236701747 (88716762ms, 84704816160 leaves / s)
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 33407185984
//info string Transposition table stores successful = 1698899668 (5.09%)
//info string Transposition table probes = 34929510398
//info string Transposition table probes successful = 1522311298 (4.36%)
//info string Positions counted in search = 28842629271840 (0.05%)
//info string Positions counted from transposition table = 62826126607429907 (99.95%)


// 0.88 hours {52.7 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
// Without extra 16-bit hash
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (3162636ms, 663260332108 leaves / s)
//info string Transposition table node count overflows = 0
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1336935897
//info string Transposition table stores successful = 903613929 (67.59%)
//info string Transposition table probes = 2166927599
//info string Transposition table probes successful = 829991701 (38.30%)
//info string Positions counted in search = 1072412984146 (0.05%)
//info string Positions counted from transposition table = 2096578590712660 (99.95%)


// 0.67 hours {40.1 minutes} (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
//setoption name hash value 16384
//info string Transposition table memory set to 16384MB
//setoption name threads value 16
//info string Threads set to 16
//go perft 11
//info string a2a3 : 60403292887824
//info string b2b3 : 79510326025357
//info string c2c3 : 92235553734553
//info string d2d3 : 151857971385067
//info string e2e3 : 241074613621302
//info string f2f3 : 51614296095395
//info string g2g3 : 82762826570051
//info string h2h3 : 60097879424719
//info string a2a4 : 85054341127064
//info string b2b4 : 80419308561211
//info string c2c4 : 103605670223681
//info string d2d4 : 211583204457112
//info string e2e4 : 245841494675197
//info string f2f4 : 68372448303691
//info string g2g4 : 73966186324024
//info string h2h4 : 86739921618220
//info string b1a3 : 70080800068168
//info string b1c3 : 91451554526572
//info string g1f3 : 89933046388964
//info string g1h3 : 71046267678634
//info string Total : 2097651003696806 (2406625ms, 871615230331 leaves / s)
//info string Transposition table node count overflows = 0
//info string Transposition table hashfull = 1000
//info string Transposition table stores = 1275778402
//info string Transposition table stores successful = 842209629 (66.02%)
//info string Transposition table probes = 2058657086
//info string Transposition table probes successful = 782878683 (38.03%)
//info string Positions counted in search = 1019151131485 (0.05%)
//info string Positions counted from transposition table = 2096631852565321 (99.95%)
//info string Transposition entries by draft : 11 = 0 10 = 20 9 = 400 8 = 5362 7 = 72078 6 = 822518 5 = 9417681 4 = 96379994 3 = 764977497 2 = 202066274


//----------------------------------------------------------------------------------------------------

Perft::PerftTranspositionTableBucket_Struct* Perft::PerftTranspositionTablePointer = nullptr;
uint32_t Perft::PerftTranspositionTableBuckets = 0;
uint32_t Perft::PerftTranspositionTableBucketsMask;
int Perft::PerftDepth;
bool Perft::PerftSilent;

Perft::PerftResult_Struct ThreadResults[ThreadsMax];

//----------------------------------------------------------------------------------------------------

#pragma region TT routines

__declspec(noinline)
void Perft::ClearPerftTranspositionTable()
{
	//std::chrono::time_point<std::chrono::steady_clock> StartClock = std::chrono::steady_clock::now();

	// These nested loops below with multiple assignments can be very slow when clearing huge tables! e.g. a 16GB table takes about 3.2s
	//for (uint32_t bucket = 0; bucket < PerftTranspositionTableBuckets; bucket++)
	//	for (uint32_t entry = 0; entry < PerftTranspositionTableEntriesPerBucket; entry++)
	//	{
	//		PerftTranspositionTablePointer[bucket].Entries[entry].hash64 = 0; // Setting the hash to zero doesn't really 'clear' it (because it's a valid value) but it's useful for visual debugging!
	//		PerftTranspositionTablePointer[bucket].Entries[entry].subTreeDepth = 0; // A value of 0 is never used in the search so it can be considered as 'unused'
	//		PerftTranspositionTablePointer[bucket].Entries[entry].locked = false;
	//	}

	// This loop is over twice as fast! e.g. a 16GB table takes about 1.5s
	//for (uint32_t ui64s = 0; ui64s < PerftTranspositionTableBuckets * 8; ui64s++)
	//	((uint64_t*)PerftTranspositionTablePointer)[ui64s] = 0ULL;

	// memset uses optimised code which is comparable to the above loop
	memset(PerftTranspositionTablePointer, 0, PerftTranspositionTableBuckets * sizeof(PerftTranspositionTableBucket_Struct));

	// N.B. once the memory has been cached this executes about 2 to 3 times faster! But still a good reason not to use excessively large tables!

	//Output("info string Time: " + MyUI64TOA(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - StartClock).count()) + "ms");
}

__declspec(noinline)
void Perft::AllocatePerftTranspositionTable()
{
	assert(sizeof(PerftTranspositionTableBucket_Struct) == 64);
	assert(sizeof(PerftTranspositionTableEntry_Struct) == 16);

	// Calculate the largest 'power of 2' number of entries/buckets that will fit in the specified number of bytes
	PerftTranspositionTableBuckets = 1;
	while ((PerftTranspositionTableBuckets * sizeof(PerftTranspositionTableBucket_Struct)) <= (TranspositionTableMemory * 1024ULL * 1024ULL))
		PerftTranspositionTableBuckets <<= 1;
	PerftTranspositionTableBuckets >>= 1;
	// N.B. Increasing the transposition table size may be counter-productive beyond some margin.
	// Once the table is not being completely filled after the search you are just storing the same info spread over more memory.
	// Some testing indicates that once you get more than about 50% of the table not being used you will suffer a slow down.

	// Free any previously allocated memory. If the pointer is nullptr it does nothing.
	AlignedFreeMemory(PerftTranspositionTablePointer);
	PerftTranspositionTablePointer = nullptr;

	// Allocate transposition table memory
	if (PerftTranspositionTableBuckets > 0)
	{
		PerftTranspositionTableBucketsMask = PerftTranspositionTableBuckets - 1;
		if (LargePagesAvailable)
		{
			PerftTranspositionTablePointer = (PerftTranspositionTableBucket_Struct*)VirtualAlloc(NULL, PerftTranspositionTableBuckets * sizeof(PerftTranspositionTableBucket_Struct), MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
			if (PerftTranspositionTablePointer == nullptr)
			{
				Output("info string *** Error! Perft 'large pages' transposition table memory could not be allocated! Falling back to standard pages.");
				OutputError("Perft 'large pages' transposition table memory could not be allocated! Falling back to standard pages.");
			}
		}
		if (PerftTranspositionTablePointer == nullptr)
			PerftTranspositionTablePointer = (PerftTranspositionTableBucket_Struct*)AlignedAllocateMemory(PerftTranspositionTableBuckets * sizeof(PerftTranspositionTableBucket_Struct), 64);
		if (PerftTranspositionTablePointer == nullptr)
		{
			Output("info string *** Error! Perft transposition table memory could not be allocated!");
			OutputError("Perft transposition table memory could not be allocated!");
			PerftTranspositionTableBuckets = 0;
		}
	}
	if (IsDebug && (PerftTranspositionTablePointer != nullptr))
	{
		Output("info string Transposition table memory = " + MyUI64TOA(TranspositionTableMemory) + "MB (" + MyUI64TOA(TranspositionTableMemory * 1024ULL * 1024ULL) + " bytes)");
		Output("info string Perft transposition table bucket size = " + MyUI64TOA(sizeof(PerftTranspositionTableBucket_Struct)) + " bytes");
		Output("info string Perft transposition table entry size = " + MyUI64TOA(sizeof(PerftTranspositionTableEntry_Struct)) + " bytes");
		Output("info string Perft transposition table entries per bucket = " + MyUI64TOA(PerftTranspositionTableEntriesPerBucket));
		Output("info string Perft transposition table buckets = " + MyUI64TOA(PerftTranspositionTableBuckets));
		Output("info string Perft transposition table entries = " + MyITOA(PerftTranspositionTableBuckets * PerftTranspositionTableEntriesPerBucket));
		Output("info string Perft transposition table memory allocated = " + MyUI64TOA(PerftTranspositionTableBuckets * sizeof(PerftTranspositionTableBucket_Struct) / (1024ULL * 1024ULL)) + "MB (" + MyUI64TOA(PerftTranspositionTableBuckets * sizeof(PerftTranspositionTableBucket_Struct)) + " bytes)");
	}
}

__declspec(noinline)
uint32_t Perft::HashfullPerftTranspositionTable()
{
	if (PerftTranspositionTableBuckets == 0)
		return 0;

	// Computes the UGI Hashfull value
	// Assuming an even distribution of used entries across the entire table a fairly accurate estimate can be made by examining a small subset of entries
	// Even with the smallest possible transposition table (1MB) we would still have 16384 buckets
	// Examining exactly 1000 entries avoids any scaling maths on return
	uint32_t usedEntries = 0;
	uint32_t bucketsToTry = 1000 / PerftTranspositionTableEntriesPerBucket;
	for (uint32_t bucket = 0; bucket < bucketsToTry; bucket++)
		for (uint32_t entry = 0; entry < PerftTranspositionTableEntriesPerBucket; entry++)
			if (PerftTranspositionTablePointer[bucket].Entries[entry].data != 0)
				usedEntries++;
	//return (uint32_t)((usedEntries * 1000) / (bucketsToTry * PerftTranspositionTableEntriesPerBucket));
	return usedEntries;
}

__declspec(noinline)
std::string Perft::StatsPerftTranspositionTable()
{
	// For lower depths these numbers will accurately represent the number of unique positions at that depth
	// However at higher depths (even with huge tables) the numbers may be a bit off as entries may have overwritten each other
	uint64_t counts[64];
	for (int index = 0; index < 64; index++)
		counts[index] = 0;

	for (uint32_t bucket = 0; bucket < PerftTranspositionTableBuckets; bucket++)
		for (uint32_t entry = 0; entry < PerftTranspositionTableEntriesPerBucket; entry++)
		{
			int subTreeDepth = (PerftTranspositionTablePointer[bucket].Entries[entry].data >> 56) & subTreeDepthMask;
			counts[PerftDepth - subTreeDepth - 1]++;
		}

	std::string result = "";
	for (int index = 0; index < PerftDepth - 1; index++)
		result += MySI64TOA(index) + "=" + MyUI64TOA(counts[index]) + " ";

	return result;
}

void Perft::AddToPerftTranspositionTable(PerftTranspositionTableEntry_Struct* tte0, uint8_t depthRemaining, uint64_t nodes64)
{
	if (PerftTranspositionTableBuckets > 0)
	{
		// N.B. checkmate and stalemate positions have zero node counts but it's still marginally faster to save them as it saves the move generation
		if (nodes64 > nodesMask) // Don't store the position if the number of nodes is greater than we can handle! (56 bits)
		{
			perftNodesOverflows++;
			return;
		}

		perftStores++;
		uint64_t hash64 = perftBrain.gameRecordPointer->transpositionTableHash64WithEP;

		// Find candidate entry for replacement
		int entryToReplace;
		int shallowestSubTreeDepth = 999;
		uint64_t fewestNodes; // = ULONG_MAX; No need to initialise this
		uint8_t oldestTranspositionTableAge = (TranspositionTableAge + 1) & TTFlagAgeMask;
		// N.B. When single-theading, this exact position cannot already be in the TT as the probe at the start of the node would have retrieved and used it
		// When multi-threading, more than one thread could have written this entry before exiting the node
		for (int entry = 0; entry < PerftTranspositionTableEntriesPerBucket; entry++)
		{
			uint64_t tteData = tte0[entry].data;
			uint64_t tteHash = tte0[entry].hash64 ^ tteData;
			uint64_t tteNodes = tteData & nodesMask;
			uint8_t tteSubTreeDepth = (tteData >> 56) & subTreeDepthMask;
			uint8_t tteAge = (tteData >> 62) & ageMask;
			
			if ((tteHash == hash64) && (tteSubTreeDepth == depthRemaining)) // Do we already have this position (with the correct subTreeDepth) in the table?
			{
				// This will only happen when multi-threading so we can just return as we've already got the info
				if (tteNodes != nodes64) // Do the threads disagree? :O
					OutputError("Perft counts disagree!!!");
				return;
			}
			
			//if (tteAge == oldestTranspositionTableAge) // Aged entry? (Replacing immediately seems to work best!)
			//{
			//	shallowestSubTreeDepth = 0;
			//	entryToReplace = entry;
			//}
			//else
			if (tteSubTreeDepth < shallowestSubTreeDepth) // Happens at least once as shallowestSubTreeDepth initialised to 999
			{
				shallowestSubTreeDepth = tteSubTreeDepth;
				fewestNodes = tteNodes;
				entryToReplace = entry;
			}
			else if ((tteSubTreeDepth == shallowestSubTreeDepth) && (tteNodes < fewestNodes)) // fewestNodes will always have been set at least once in the clause above
			{
				// This extra clause saves ~ 2% - 5% !
				fewestNodes = tteNodes;
				entryToReplace = entry;
			}
		}
		assert((entryToReplace >= 0) && (entryToReplace < PerftTranspositionTableEntriesPerBucket));

		if (depthRemaining >= shallowestSubTreeDepth) // Replace if the current subtree depth is '>=' the stored subtree depth (seems to perform better than '>' or 'always')
		{
			//uint64_t newData = nodes64 | ((uint64_t)depthRemaining << 56) | ((uint64_t)TranspositionTableAge << 62);
			uint64_t newData = nodes64 | ((uint64_t)depthRemaining << 56);
			tte0[entryToReplace].data = newData;
			tte0[entryToReplace].hash64 = hash64 ^ newData;
			perftStoresSuccessful++;
		}
	}
}

#pragma endregion

//----------------------------------------------------------------------------------------------------

void Perft::TreeSearchPerft(int ply, int sideToMove, int isInCheck)
{
	// This recursive Perft routine uses the standard generate/make/unmake routines (rather than any slimlime version) which does mean some minimal overhead e.g. up-/down-dating material balances, etc
	// but what would be the point of testing routines that you don't use in the main search! :)
	// I have confirmed the correctness from the original position on Perft(11) which it completed in about...
	// 4.4 hours (4GB hash, 8 threads, AMD Ryzen 5 3600 6-Core Processor 3.59 GHz)
	// 1.8 hours (4GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)
	// 0.9 hours (16GB hash, 16 threads, AMD Ryzen 9 7950X 16-Core Processor 4.50 GHz)

	assert(CompareMailboxBoard64ToPiecesBB(perftBrain.mailboxBoard64, perftBrain.piecesBB));
	assert((PopulationCountX(perftBrain.piecesBB[0][King]) == 1) && (PopulationCountX(perftBrain.piecesBB[1][King]) == 1));
	assert((PopulationCountX(perftBrain.piecesBB[0][Queen]) <= 9) && (PopulationCountX(perftBrain.piecesBB[1][Queen]) <= 9));
	assert((PopulationCountX(perftBrain.piecesBB[0][Rook]) <= 10) && (PopulationCountX(perftBrain.piecesBB[1][Rook]) <= 10));
	assert((PopulationCountX(perftBrain.piecesBB[0][Bishop]) <= 10) && (PopulationCountX(perftBrain.piecesBB[1][Bishop]) <= 10));
	assert((PopulationCountX(perftBrain.piecesBB[0][Knight]) <= 10) && (PopulationCountX(perftBrain.piecesBB[1][Knight]) <= 10));
	assert((PopulationCountX(perftBrain.piecesBB[0][Pawn]) <= 8) && (PopulationCountX(perftBrain.piecesBB[1][Pawn]) <= 8));
	assert(perftBrain.piecesBB[0][AllPieces] == (perftBrain.piecesBB[0][Pawn] | perftBrain.piecesBB[0][Knight] | perftBrain.piecesBB[0][Bishop] | perftBrain.piecesBB[0][Rook] | perftBrain.piecesBB[0][Queen] | perftBrain.piecesBB[0][King]));
	assert(perftBrain.piecesBB[1][AllPieces] == (perftBrain.piecesBB[1][Pawn] | perftBrain.piecesBB[1][Knight] | perftBrain.piecesBB[1][Bishop] | perftBrain.piecesBB[1][Rook] | perftBrain.piecesBB[1][Queen] | perftBrain.piecesBB[1][King]));
	assert(perftBrain.gameRecordPointer->transpositionTableHash64 == ((sideToMove == 0) ? GenerateTranspositionTableHash64(perftBrain.mailboxBoard64, perftBrain.gameRecordPointer) : ~GenerateTranspositionTableHash64(perftBrain.mailboxBoard64, perftBrain.gameRecordPointer)));
	assert(perftBrain.gameRecordPointer->transpositionTableHash64WithEP == (perftBrain.gameRecordPointer->transpositionTableHash64 ^ TranspositionTableRandomsEnPassant[perftBrain.gameRecordPointer->epSquare]));
	assert((ply >= 1) && (ply < MaximumPly));
	assert((sideToMove >= 0) && (sideToMove < Sides));

	//----------------------------------------------------------------------------------------------------

	MoveWithScore_Struct moveList[220];

	//----------------------------------------------------------------------------------------------------

	// Is this position in the transposition table?
	PerftTranspositionTableEntry_Struct* tte0; // No need to initialise this
	if (PerftTranspositionTableBuckets > 0)
	{
		perftProbes++;
		uint64_t hash64 = perftBrain.gameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (PerftTranspositionTableEntry_Struct*)(PerftTranspositionTablePointer + (hash64 & PerftTranspositionTableBucketsMask));

		for (int entry = 0; entry < PerftTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
		{
			uint64_t tteData = tte0[entry].data;
			uint64_t tteHash = tte0[entry].hash64 ^ tteData;
			//if ((hash == hash64) && (((data >> 40) & subTreeDepthMask) == PerftDepth - ply))
			if ((tteHash == hash64) && ((int)((tteData >> 56) & subTreeDepthMask) == PerftDepth - ply))
			{
				//uint64_t occupied64 = perftBrain.piecesBB[0][AllPieces] | perftBrain.piecesBB[1][AllPieces];
				//uint32_t occupied32 = (uint32_t)(occupied64 >> 32) ^ (uint32_t)occupied64;
				//uint16_t occupied16 = (uint16_t)(occupied32 >> 16) ^ (uint16_t)occupied32;
				//if ((data >> 48) == occupied16) // Verify the additional 'hash' to alleviate observed type-1 errors
				{
					uint64_t tteNodes = (tteData & nodesMask);
					perftPositionsFromTranspositionTable += tteNodes;
					perftNodes += tteNodes;
					perftProbesSuccessful++;
					//if (((data >> 46) & ageMask) != TranspositionTableAge) // Touch the age for aged entries
					//{
					//	data = data & ~(3ULL << 46);
					//	data = data | ((uint64_t)TranspositionTableAge << 46);
					//	tte0[entry].data = data;
					//	tte0[entry].hash64 = hash64 ^ data;
					//}
					return;
				}
				break;//?????
			}
		}
	}

	//----------------------------------------------------------------------------------------------------

	uint64_t initialPerftNodes = perftNodes;

	// Generate move list
	perftBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
	uint32_t movesCount = perftBrain.GenerateAllMoves(sideToMove, isInCheck, moveList);

	// Loop through move list
	int enemyKingSquare = GetLS1BIndex(perftBrain.piecesBB[sideToMove ^ 1][King]);

	// Try to get any extra threads working on different moves in the list by offsetting and reversing
	int start = 0;
	int increment = 1;
	if (movesCount > 1)
	{
		start = (movesCount * ThreadId) / Threads; // Distribute the starting index for each thread evenly across the move list
		if (ThreadId & 1) // I don't know WHY reversing the search direction of alternate threads works but tests show that it does!
		{
			start = movesCount - start - 1;
			increment = -1;
		}
	}

	for (uint32_t moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
	{
		// Get next move
		perftBrain.gameRecordPointer->move.ui32 = moveList[start].ui32;

		// Adjust index
		start += increment;
		if (start < 0)
			start = movesCount - 1;
		else if (start >= (int)movesCount)
			start = 0;

		// Up-date move
		perftBrain.MakeMove(sideToMove);

		if (ply == PerftDepth - 1) // Putting this clause here saves the recursive call at the final ply (about 3% speedup) but stops 'go perft 1' working! So that is handled uniquely outside the root call
		{
			// Batch counting at the final ply
			// It turns out that using the TT at the final ply is much slower than just counting the moves!
			perftBrain.CalculatePinnedPieces(sideToMove ^ 1); // Required for legal move generation
			perftNodes += perftBrain.CountAllMoves(sideToMove ^ 1, perftBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove));
		}
		else
		{
			// Initiate the retrieval of the next transposition table cache line as soon as possible
			_mm_prefetch((char*)(PerftTranspositionTablePointer + (perftBrain.gameRecordPointer->transpositionTableHash64 & PerftTranspositionTableBucketsMask)), _MM_HINT_T0);
			TreeSearchPerft(ply + 1, sideToMove ^ 1, perftBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove));
		}

		// Down-date move
		perftBrain.UnMakeMove(sideToMove);

		//----------------------------------------------------------------------------------------------------

		// Interrupted?
		if (StopImmediately)
			return;

		//----------------------------------------------------------------------------------------------------

		// Output the counts for the current root move
		if (ply == 1)
		{
			if (
				!PerftSilent
				&& ((ThreadId == 0) || IsDebug)
				)
			{
				std::string s = "info string " + MoveNotation(perftBrain.gameRecordPointer->move.ui32);
				s += ": " + MyUI64TOA(perftNodes - previousPerftNodes);
				if (IsDebug)
				{
					nowClock = std::chrono::steady_clock::now();
					uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(nowClock - previousClock).count();
					previousClock = nowClock;

					s += " (" + MyUI64TOA(ms) + "ms, Hashfull = " + MyITOA(HashfullPerftTranspositionTable()) + ", ThreadId = " + MyITOA(ThreadId) + " CurrentProcessorNumber = " + std::to_string(GetCurrentProcessorNumber()) + ")";
				}
				previousPerftNodes = perftNodes;
				Output(s);
			}
		}
	} // (Loop through move list)

	// N.B. If a helper thread finishes before the main thread starts (can happen on very lower Perft values) it won't display any root move breakdown because the main thread just reads the entire solution out of the TT!
	AddToPerftTranspositionTable(tte0, PerftDepth - ply, perftNodes - initialPerftNodes);
}

//----------------------------------------------------------------------------------------------------

Perft::PerftResult_Struct Perft::ComputePerft()
{
	// At the start of these Compute* routines only the 64-square mailbox board and game record are set up in the outer engine brain

	perftBrain.CopyFrom(&EngineBrain);

	// Set up the bit boards from the 64-square mailbox board
	ConvertMailboxBoard64ToPiecesBB(perftBrain.mailboxBoard64, perftBrain.piecesBB);

	//----------------------------------------------------------------------------------------------------

	// Get timer
	uint64_t ms = 0;
	startClock = std::chrono::steady_clock::now();
	previousClock = startClock;

	// Initialise any variables required for the search
	perftBrain.gameRecordPointer = &perftBrain.gameRecord[perftBrain.GameRecordIndexRoot];
	uint64_t hash64 = GenerateTranspositionTableHash64(perftBrain.mailboxBoard64, perftBrain.gameRecordPointer);
	if (SideToMove == 1)
		hash64 = ~hash64;
	perftBrain.gameRecordPointer->transpositionTableHash64 = hash64;
	perftBrain.gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[perftBrain.gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0

	// Clear Perft counts
	perftNodes = 0;
	previousPerftNodes = 0;
	perftNodesOverflows = 0;
	perftStores = 0;
	perftStoresSuccessful = 0;
	perftProbes = 0;
	perftProbesSuccessful = 0;
	perftPositionsFromTranspositionTable = 0;

	// Do the perft search
	if (PerftDepth == 1)
	{
		// An optimisation in the TreeSearchPerft routine stops 'go perft 1' working! So that is handled uniquely here
		perftBrain.CalculatePinnedPieces(SideToMove); // Required for legal move generation
		perftNodes += perftBrain.CountAllMoves(SideToMove, perftBrain.IsEnemyKingAttacked(GetLS1BIndex(perftBrain.piecesBB[SideToMove][King]), SideToMove ^ 1));
	}
	else
		TreeSearchPerft(1, SideToMove, perftBrain.IsEnemyKingAttacked(GetLS1BIndex(perftBrain.piecesBB[SideToMove][King]), SideToMove ^ 1));

	ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startClock).count();

	PerftResult_Struct pr;
	pr.ms = ms;
	pr.perftNodes = perftNodes;
	pr.perftNodesOverflows = perftNodesOverflows;
	pr.perftStores = perftStores;
	pr.perftStoresSuccessful = perftStoresSuccessful;
	pr.perftProbes = perftProbes;
	pr.perftProbesSuccessful = perftProbesSuccessful;
	pr.perftPositionsFromTranspositionTable = perftPositionsFromTranspositionTable;

	return pr;
}

//----------------------------------------------------------------------------------------------------

void Perft::ComputePerftMTLaunchHelperThread(int threadId)
{
	Perft* ts;
	ts = new Perft;
	ts->ThreadId = threadId;
	ThreadResults[threadId].perftNodes = 0;
	PerftResult_Struct pr = ts->ComputePerft();
	ThreadResults[threadId] = pr;
	delete ts;
}

Perft::PerftResult_Struct Perft::ComputePerftMT(bool clearTT)
{
	// If we've got some unused transposition table memory then allocate it
	if ((TranspositionTableMemory > 0) && (PerftTranspositionTableBuckets == 0))
	{
		Perft::AllocatePerftTranspositionTable();
		if (!clearTT)
			Perft::ClearPerftTranspositionTable(); // If we're not going to clear the TT every position we must clear it upon creation
	}

	// Initialise transposition table
	if (clearTT)
		Perft::ClearPerftTranspositionTable(); // This is the main overhead when processing a file!

	// Advance the TT age
	TranspositionTableAge++;
	TranspositionTableAge &= TTFlagAgeMask;
	assert((TranspositionTableAge >= 0) && (TranspositionTableAge <= 3));

	StopImmediately = false;

	ClearAnalysisCounters();

	// Launch any helper threads independently
	// (If the number of helper threads > logical CPU threads this can take a discernible time)
	// Multi-threaded timing (on my AMD Ryzen 9 7950X) can be very variable! It seems to depend on the OS choice of cores/threads which seem to be selected when the program is launched.
	// Watching the core activity in Ryzen Master you can almost immediately identify if it's a 'fast' or a 'slow' load.
	// Using SetThreadIdealProcessor (or processor affinity) seems to give more consistent (fast) results but can still sometimes have no effect if the program loads in 'slow mode'.
	//uint32_t hardwareThreadsMax = std::thread::hardware_concurrency();
	std::thread threads[ThreadsMax];
	for (int threadId = 1; threadId < Threads; threadId++)
	{
		threads[threadId] = std::thread(ComputePerftMTLaunchHelperThread, threadId);
		//DWORD result = SetThreadIdealProcessor(threads[threadId].native_handle(), threadId % hardwareThreadsMax);
	}

	// Compute the result in this main thread which uses the Perft class instance declared in Engine
	EnginePerft.ThreadId = 0;
	ThreadResults[0].perftNodes = 0;
	PerftResult_Struct pr = EnginePerft.ComputePerft();
	ThreadResults[0] = pr;

	// Wait for helper threads to finish
	for (int threadId = 1; threadId < Threads; threadId++)
		threads[threadId].join();

	// Display final Perft counts
	if (StopImmediately)
		Output("info string Interrupted!");
	else if (!PerftSilent)
	{
		uint64_t ms = std::max(pr.ms, 1ULL); // To avoid a divide-by-zero error when calculating the nodes/second!
		Output(
			"info string Total: "
			+ MyUI64TOA(pr.perftNodes)
			+ " (" + MyUI64TOA(ms) + "ms, " + MyUI64TOA(pr.perftNodes * 1000 / ms) + " leaves/s)"
		);
		if (PerftTranspositionTableBuckets > 0)
		{
			Output("info string Transposition table node count overflows = " + MyUI64TOA(pr.perftNodesOverflows));
			Output("info string Transposition table hashfull = " + MyITOA(HashfullPerftTranspositionTable()));
			Output("info string Transposition table stores = " + MyUI64TOA(pr.perftStores));
			Output("info string Transposition table stores successful = " + MyUI64TOA(pr.perftStoresSuccessful) + " (" + MyFTOA((float(pr.perftStoresSuccessful) * 100) / pr.perftStores) + "%)");
			Output("info string Transposition table probes = " + MyUI64TOA(pr.perftProbes));
			Output("info string Transposition table probes successful = " + MyUI64TOA(pr.perftProbesSuccessful) + " (" + MyFTOA((float(pr.perftProbesSuccessful) * 100) / pr.perftProbes) + "%)");
			Output("info string Positions counted in search = " + MyUI64TOA(pr.perftNodes - pr.perftPositionsFromTranspositionTable) + " (" + MyFTOA((float(pr.perftNodes - pr.perftPositionsFromTranspositionTable) * 100) / pr.perftNodes) + "%)");
			Output("info string Positions counted from transposition table = " + MyUI64TOA(pr.perftPositionsFromTranspositionTable) + " (" + MyFTOA((float(pr.perftPositionsFromTranspositionTable) * 100) / pr.perftNodes) + "%)");
			Output("info string Transposition entries by depth: " + StatsPerftTranspositionTable());
		}
		Output("");

		// Confirm every thread returned the same result
		bool error = false;
		for (int threadId = 1; threadId < Threads; threadId++)
			if (ThreadResults[threadId].perftNodes != ThreadResults[0].perftNodes)
			{
				error = true;
				break;
			}
		if (error)
		{
			std::string results = "Perft thread results not consistent! ";
			for (int threadId = 0; threadId < Threads; threadId++)
				results += MyITOA(threadId) + " = " + MyUI64TOA(ThreadResults[threadId].perftNodes) + "\n";
			Output(results);
			OutputError(results);
		}
	}

	DisplayAnalysisCounters();

	return pr;
}

void Perft::ComputePerftFile(std::string filename)
{
	char line[10000];
	std::string tokens[20];
	int tokenCount;
	int positionsCount;
	int errors = 0;
	FILE *PerftFile;
	uint64_t ms = 0;

	// Take input from file
	fopen_s(&PerftFile, filename.c_str(), "r");
	if (PerftFile == NULL)
		Output("*** Error! " + filename + " not found");
	else
	{
		StopWhenIterationComplete = false;

		positionsCount = 1;
		while ((fgets(line, 10000, PerftFile) != NULL) && (!StopWhenIterationComplete))
		{
			line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
			Split(line, &tokens[0], &tokenCount, ",");
			std::string s;
			s = "position fen " + tokens[0];
			SetPositionAndMoves(s);

			PerftResult_Struct pr = ComputePerftMT(false);

			Output("info string " + MyITOA(positionsCount) + ": " + tokens[0] + " : " + MyUI64TOA(pr.perftNodes) + " (" + MyUI64TOA(pr.ms) + "ms)");
			ms += pr.ms;

			if (MyUI64TOA(pr.perftNodes) != tokens[PerftDepth])
			{
				// If the value we just computed doesn't match the given correct value given in the file then output an error message
				Output("*** Error!");
				Output("Calculated: " + MyUI64TOA(pr.perftNodes));
				Output("Correct:    " + tokens[PerftDepth]);
				errors++;
			}

			positionsCount++;
		}

		fclose(PerftFile);

		// Show closing statistics
		Output("info string Time: " + MyUI64TOA(ms) + "ms");
		Output("info string errors = " + MyITOA(errors));
		Output("");
	}
}

// This is the UGI entry point when it receives a 'go perft N' command
void Perft::ComputePerftWrapper()
{
	PerftDepth = TC.PerftN;

	if (TC.PerftFilename == "")
	{
		PerftSilent = false;
		ComputePerftMT(true);
	}
	else
	{
		// Process Perft commands from a file
		PerftSilent = true;
		ComputePerftFile(TC.PerftFilename);
	}

	ComputingMove = false;
}

#ifdef EXPERIMENTAL

// The number of unique positions by depth can be found here https://oeis.org/search?q=20+400+5362+72078&language=english&go=Search

void Perft::ComputePerftUnique()
{
	char line[10000];
	std::string tokens[1000];
	int tokenCount;
	int errors = 0;
	FILE *PerftFile;
	FILE *PerftOutputFile;
	uint64_t ms = 0;

	FILE *PerftNextFile;
	int frequency;
	int lineNumber;
	//fopen_s(&PerftNextFile, ((std::string)"D:\\Developer\\Games\\UGIConsoles\\CC20NNx\\Solution\\x64\\Release\\Perft\\Perft8FENsUniqueAllByFrequency\\Next.txt").c_str(), "r");
	fopen_s(&PerftNextFile, ((std::string)"D:\\Chess\\Perft\\Perft8FENsUniqueAllByFrequencyTEST\\Next.txt").c_str(), "r");
	fgets(line, 10000, PerftNextFile);
	frequency = atoi(line);
	fgets(line, 10000, PerftNextFile);
	lineNumber = atoi(line);
	fclose(PerftNextFile);

	uint64_t totalms = 0;

nextfile:
	//std::string filename = "D:\\Developer\\Games\\UGIConsoles\\CC20NNx\\Solution\\x64\\Release\\Perft\\Perft8FENsUniqueAllByFrequency\\Perft8FENsUniqueAllByFrequency0" + MyITOA(frequency) + ".txt";
	std::string filename = "D:\\Chess\\Perft\\Perft8FENsUniqueAllByFrequencyTEST\\Perft8FENsUniqueAllByFrequency0" + MyITOA(frequency) + ".txt";
	fopen_s(&PerftFile, filename.c_str(), "r");
	if (PerftFile == NULL)
	{
		frequency--;
		if (frequency == 0)
			goto exit;
		goto nextfile;
	}
	Output("\n" + filename + "\n");
	while (fgets(line, 10000, PerftFile) != NULL)
	{
		line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
		Split(line, &tokens[0], &tokenCount, ",");

		std::string s;
		s = "position fen " + tokens[0];
		SetPositionAndMoves(s);

		//Perft::PerftDepth = 8;
		PerftDepth = 8;
		//Perft::PerftResult_Struct pr = ComputePerftMT(false); // Because the positions are related, allow the TT entries to persist DOESN'T SEEM TO WORK!!!
		PerftResult_Struct pr = ComputePerftMT(false); // Because the positions are related, allow the TT entries to persist DOESN'T SEEM TO WORK!!!
		//Perft::PerftResult_Struct pr = ComputePerftMT(true); // Because the positions are related, allow the TT entries to persist DOESN'T SEEM TO WORK!!!
		totalms += pr.ms;

		//PerftOutputFile = fopen(((std::string)"D:\\Developer\\Games\\UGIConsoles\\CC20NNx\\Solution\\x64\\Release\\Perft\\Perft8FENsUniqueAllByFrequency\\Perft8FENsUniqueAllByFrequency0" + MyITOA(frequency) + "Results.txt").c_str(), "a");
		PerftOutputFile = fopen(((std::string)"D:\\Chess\\Perft\\Perft8FENsUniqueAllByFrequencyTEST\\Perft8FENsUniqueAllByFrequency0" + MyITOA(frequency) + "Results.txt").c_str(), "a");
		s = (std::string)line + "," + MyUI64TOA(pr.perftNodes) + "\n";
		fprintf(PerftOutputFile, "%s", s.c_str());
		fclose(PerftOutputFile);
	}
	fclose(PerftFile);
	frequency--;
	if (frequency > 0)
		goto nextfile;

exit:
	Output("Done! " + MyUI64TOA(totalms));
}

void Perft::ComputePerftUnique2()
{
	char line[10000];
	std::string tokens[1000];
	int tokenCount;
	int errors = 0;
	FILE *PerftFile;
	uint64_t ms = 0;

	int frequency;
	int lineNumber;
	frequency = 14363;
	//frequency = 5000;
	lineNumber = 1;

	uint64_t files = 0;
	uint64_t uniquePositions = 0;
	uint64_t totalPositions = 0;
	long double totalPerftCount = 0;
	uint64_t totalPerftCount2 = 0;

nextfile:
	std::string frequencyString = MyITOA(frequency);
	if (frequency < 10000)
	{
		frequencyString = "0" + frequencyString;
		if (frequency < 1000)
		{
			frequencyString = "0" + frequencyString;
			if (frequency < 100)
			{
				frequencyString = "0" + frequencyString;
				if (frequency < 10)
					frequencyString = "0" + frequencyString;
			}
		}
	}
	fopen_s(&PerftFile, ((std::string)"D:\\Chess\\Perft\\Perft8FENsUniqueAllByFrequency\\Perft8FENsUniqueAllByFrequency" + frequencyString + "Results.txt").c_str(), "r");
	if (PerftFile == NULL)
	{
		frequency--;
		if (frequency == 0)
			goto exit;
		goto nextfile;
	}
	while (fgets(line, 10000, PerftFile) != NULL)
	{
		line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
		Split(line, &tokens[0], &tokenCount, ",");

		uniquePositions++;
		uint64_t positions = std::stoull(tokens[1].c_str());
		uint64_t perftCount = std::stoull(tokens[2].c_str());
		//if (perftCount > 0) SHOULD WE INCLUDE OR EXCLUDE 'MATED' POSITIONS IN THE CALCULATIONS???
		{
			totalPositions += positions;
			totalPerftCount += (long double)(positions * perftCount);
		}
	}
	fclose(PerftFile);
	files++;

	Output("Files=" + MyUI64TOA(files) + ", Unique Positions=" + MyUI64TOA(uniquePositions) + "(" + MyDTOA(((long double)(uniquePositions * 100)) / 988187354) + "%), Total Positions=" + MyUI64TOA(totalPositions) + "(" + MyDTOA(((long double)(totalPositions * 100)) / 84998978956) + "%), Total Perft Count=" + MyDTOA(totalPerftCount));
	//Output("Average Perft Count=" + std::to_string(totalPerftCount / totalPositions) + ", Estimated Perft(16)=" + std::to_string((totalPerftCount / totalPositions) * 84998978956));
	Output("Average Perft Count=" + std::to_string(totalPerftCount / totalPositions) + ", Estimated Perft(16)=" + std::to_string(((totalPerftCount * 84998978956) / totalPositions)));


	frequency--;
	if (frequency > 0)
		goto nextfile;

exit:;
}

#include <fstream>

struct UniqueFEN_struct
{
	std::string FEN;
	uint32_t count;
	UniqueFEN_struct* gt;
	UniqueFEN_struct* lt;
};

//-build hash table on fly with 2 pointers for > and < current entry... then can keep everything ordered and it's a binary chop to find an entry. incrememtn count if find a match (1st entry is initial posn)
//work from previous plies unique list
//at the end, walk the binary tree and print out the FENs to a new file

FILE *PerftFENsFileInput;
FILE *PerftFENsFileOutput;
//FILE *TEST;
uint64_t total = 0;
uint64_t lines = 0;

void UniquesByFrequency()
{
	uint32_t FENsCount = 0;
	char line[10000];
	std::string tokens[1000];
	int tokenCount;
	int positionsCount;

	fopen_s(&PerftFENsFileInput, "C:\\Perft\\Perft8FENsUniqueAll.txt", "r");

	positionsCount = 1;
	while (fgets(line, 10000, PerftFENsFileInput) != NULL)
	{
		line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
		Split(line, &tokens[0], &tokenCount, ",");
		int occurred = std::stoi(tokens[1]);

		if ((positionsCount & 65535) == 0)
			Output(MyITOA(positionsCount) + ": " + line);

		std::string filename;
		filename = "0000" + MyITOA(occurred);
		filename = filename.substr(filename.length() - 5);
		filename = "C:\\Perft\\Perft8FENsUniqueAllByFrequency\\Perft8FENsUniqueAllByFrequency" + filename + ".txt";

		std::ofstream frequencyFile;

		frequencyFile.open(filename, std::ios_base::app);
		frequencyFile << std::string(line) + "\n";
		frequencyFile.close();


		positionsCount++;
	}

	fclose(PerftFENsFileInput);
}

void PrintUniques(UniqueFEN_struct* currentNode)
{
	std::string s;
	s = currentNode->FEN + "," + MyITOA(currentNode->count) + "\n";
	total += currentNode->count;
	lines++;
	fprintf(PerftFENsFileOutput, s.c_str());
	if (currentNode->gt != NULL)
		PrintUniques(currentNode->gt);
	if (currentNode->lt != NULL)
		PrintUniques(currentNode->lt);
}

void Unique()
{
	UniquesByFrequency();
	return;

	uint32_t FENsCount = 0;
	char line[10000];
	std::string tokens[1000];
	int tokenCount;
	int positionsCount;
	MoveWithScore_Struct moveList[220];
	int sideToMove;
	UniqueFEN_struct* rootNode;
	UniqueFEN_struct* currentNode;
	//UniqueFEN_struct* previousNode;
	UniqueFEN_struct* newNode;


	int perftDepth = 7;
	fopen_s(&PerftFENsFileInput, "C:\\Perft\\Perft7FENsUnique.txt", "r");
	fopen_s(&PerftFENsFileOutput, "C:\\Perft\\Perft8FENsUniqueGT.txt", "w");
	//fopen_s(&TEST, "C:\\Perft\\TEST.txt", "w");
	//fopen_s(&PerftFENsFileInput, "D:\\Developer\\Games\\UGIConsoles\\CC20NNx\\Solution\\x64\\Release\\Perft\\Perft6FENsUnique.txt", "r");
	//fopen_s(&PerftFENsFileOutput, "D:\\Developer\\Games\\UGIConsoles\\CC20NNx\\Solution\\x64\\Release\\Perft\\Perft7FENsUnique.txt", "w");
	//fopen_s(&TEST, "D:\\Developer\\Games\\UGIConsoles\\CC20NNx\\Solution\\x64\\Release\\Perft\\TEST.txt", "w");
	////fopen_s(&PerftFENsFileInput, "Perft\\Perft0FENsUnique.txt", "r");
	////fopen_s(&PerftFENsFileOutput, "Perft\\Perft1FENsUnique.txt", "w");
	sideToMove = 1;

	FENsCount = 0;

	rootNode = new UniqueFEN_struct;
	rootNode->FEN = "rnbqk";
	rootNode->gt = NULL;
	rootNode->lt = NULL;
	//rootNode = NULL;

	// N passes
	for (int pass = 1; pass < 2; pass++)
	{

		positionsCount = 1;
		while (fgets(line, 10000, PerftFENsFileInput) != NULL)
		{
			line[strlen(line) - 1] = 0; // Remove the trailing "\n" we get when we read a line from a file
			Split(line, &tokens[0], &tokenCount, ",");
			std::string s;
			s = "position fen " + tokens[0];
			SetPositionAndMoves(s);
			int occurred = std::stoi(tokens[1]);

			if ((positionsCount & 65535) == 0)
				Output(MyITOA(positionsCount) + ": " + line);

			// Set up the bit boards from the 64-square mailbox board
			ConvertMailboxBoard64ToPiecesBB(EngineBrain.mailboxBoard64, EngineBrain.piecesBB);
			EngineBrain.gameRecordPointer = &EngineBrain.gameRecord[EngineBrain.GameRecordIndexRoot];
			//uint64_t hash64 = GenerateTranspositionTableHash64(UGIBrain.mailboxBoard64, UGIBrain.gameRecordPointer);
			//if (sideToMove == 1)
			//	hash64 = ~hash64;
			//Output(MyUI64TOA(hash64));
			//UGIBrain.gameRecordPointer->transpositionTableHash64 = hash64;
			//UGIBrain.gameRecordPointer->transpositionTableHash64WithEP = hash64 ^ TranspositionTableRandomsEnPassant[UGIBrain.gameRecordPointer->epSquare]; // N.B. TranspositionTableRandomsEnPassant[0] = 0
			bool isInCheck = EngineBrain.IsEnemyKingAttacked(BitScanForwardX(EngineBrain.piecesBB[sideToMove][King]), sideToMove ^ 1);

			EngineBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
			uint32_t movesCount = EngineBrain.GenerateAllMoves(sideToMove, isInCheck, moveList);

			//Output(MyITOA(movesCount));

			for (uint32_t moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
			{
				EngineBrain.gameRecordPointer->move.ui32 = moveList[moveListIndexIterator].ui32;
				EngineBrain.MakeMove(sideToMove);

				//IF AN EP CAPTURE 'MAY THEORETICALLY' BE POSSIBLE WE NEED TO CHECK IF THE EP CAPTURE IS LEGAL BECAUSE IF IT ISN'T IT IS ESSENTIALLY THE SAME POSN AS WHEN NO EP CAPTURE POSSIBLE
				if (EngineBrain.gameRecordPointer->epSquare != 0)
				{
					//gameRecordPointer->epSquare = currentMove->mf.toSquare + PawnMoveOffset[sideToMove ^ 1];
					bool epLegal = false;
					uint32_t kingSquare = BitScanForwardX(EngineBrain.piecesBB[sideToMove ^ 1][King]);
					Move_Struct previousMove;
					previousMove.ui32 = (EngineBrain.gameRecordPointer - 1)->move.ui32;
					if (West(UINT64SetBit(previousMove.mf.toSquare)) & EngineBrain.piecesBB[sideToMove ^ 1][Pawn])
					{
						EngineBrain.piecesBB[sideToMove ^ 1][Pawn] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare));
						EngineBrain.piecesBB[sideToMove ^ 1][AllPieces] ^= (UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare));
						EngineBrain.piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
						EngineBrain.piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
						if (!EngineBrain.IsAttacked(kingSquare, sideToMove))
							epLegal = true;
						EngineBrain.piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare);
						EngineBrain.piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare - 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare);
						EngineBrain.piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
						EngineBrain.piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
					}
					if (!epLegal)
						if (East(UINT64SetBit(previousMove.mf.toSquare)) & EngineBrain.piecesBB[sideToMove ^ 1][Pawn])
						{
							EngineBrain.piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare);
							EngineBrain.piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare);
							EngineBrain.piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
							EngineBrain.piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
							if (!EngineBrain.IsAttacked(kingSquare, sideToMove))
								epLegal = true;
							EngineBrain.piecesBB[sideToMove ^ 1][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare);
							EngineBrain.piecesBB[sideToMove ^ 1][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare + 1) ^ UINT64SetBit(EngineBrain.gameRecordPointer->epSquare);
							EngineBrain.piecesBB[sideToMove][Pawn] ^= UINT64SetBit(previousMove.mf.toSquare);
							EngineBrain.piecesBB[sideToMove][AllPieces] ^= UINT64SetBit(previousMove.mf.toSquare);
						}

					if (!epLegal)
						EngineBrain.gameRecordPointer->epSquare = 0;
				}


				std::string fen = ConvertPositionToFEN(EngineBrain.mailboxBoard64, sideToMove ^ 1, EngineBrain.gameRecordPointer->castlingStatus, EngineBrain.gameRecordPointer->epSquare, -1, -1);
				//fprintf(TEST, (fen + "\n").c_str());

				currentNode = rootNode;

				//WHEN THIS GETS INTO HUGE NUMBERS AND STARTS USING VIRTUAL MEMORY (NOT REAL MEMORY) IT GETS REALLY SLOW
				//NEED SOME WAY TO FRAGMENT THE SEARCH
				//COULD PROCESS X LINES FROM INPUT THEN DUMP OUT TREE TO FILE THEN SOMEHOW MERGE FILES LATER? BY 
				//CAN YOU WORK OUT THE AVERAGE FEN? AND PROCESS THE FENs IN PASSES, 1ST >, 2ND < ?
				//N^2 PASSES... 1ST PASS ONLY STORE GT,GT,GT, 2ND PASS GT,GT,LT, 3RD PASS GT,LT,GT ETC - WRITE RESULTS TO SEPARATE FILES FOR SANITY CHECKING - MERGE LATER. MEANS WE HAVE TO GEN MOVES N^2 TIMES
				//-DON'T FORGET THE POSNS NEAR THE ROOT OF THE ORDERED TREE (WON'T BE A PROBLEM?)
				//-WHAT SHOULD THE ROOT POSN BE THOUGH TO GET THE INITIAL GT/LT SPLIT? just the 1st posn. does it matter? yes coz you'll count it each pass
				//-WRITE ALL FENs TO SEPARATE gtgtgt, GTGTLT, ETC FILES IN ONE PASS. 2ND PASS PROCESSES EACH FILE INTO ORDERED/COUNTED TREE AND APPENDS TO FINAL RESULTS FILE. QUICKER? SLOWER? NEEDS 2x DISK SPACE
				//FOR P(11) EVEN 256 PASSES LEAVES 3 BILLION PER FILE WHICH IS TOO MUCH FOR REAL MEMORY. 1024 FILES? make dynamic based on how difficult P(N) is
				//COULD WRITE FENs TO X DIFFERENT 'ORDERED' FILES THEN ORDER/COUNT ENTRIES IN FILES LATER?
				//COULD WRITE ENTRIES MORE THAN 10 PTRs DEEP TO FILE FOR LATER PROCESSING?
				//COULD SPLIT INTO FILES BASED ON EP-SQ AND CASTLING STATUSES? BUT MOST WOULD BE -KQkq! ALSO USE #OFPIECES AS PART OF FILENAME? FEN std::string LENGTH? (NOT MUCH VARIANCE)

				//ALSO, CAN THE BELOW BE MADE MORE EFFICIENT?
			search:
				////if (currentNode == NULL)
				//if (currentNode->FEN == "")
				//{
				//	//currentNode = new UniqueFEN_struct;
				//	currentNode->FEN = fen;
				//	currentNode->count = occurred;
				//	currentNode->lt = NULL;
				//	currentNode->gt = NULL;
				//}
				//else
				{
					if (fen > currentNode->FEN)
					{
						if (currentNode->gt == NULL)
						{
							//if (currentNode == rootNode)
							//	goto exit;
							newNode = new UniqueFEN_struct;
							newNode->FEN = fen;
							newNode->count = occurred;
							newNode->gt = NULL;
							newNode->lt = NULL;
							currentNode->gt = newNode;
						}
						else
						{
							currentNode = currentNode->gt;
							goto search;
						}
					}
					else if (fen < currentNode->FEN)
					{
						if (currentNode->lt == NULL)
						{
							if (currentNode == rootNode)
								goto exit;
							newNode = new UniqueFEN_struct;
							newNode->FEN = fen;
							newNode->count = occurred;
							newNode->gt = NULL;
							newNode->lt = NULL;
							currentNode->lt = newNode;
						}
						else
						{
							currentNode = currentNode->lt;
							goto search;
						}
					}
					else
						currentNode->count += occurred;
				}
			exit:

				EngineBrain.UnMakeMove(sideToMove);
			}

			positionsCount++;
		}


		Output("Writing...");
		if (rootNode->gt != NULL)
			PrintUniques(rootNode->gt);
		Output(MyUI64TOA(lines));
		if (rootNode->lt != NULL)
			PrintUniques(rootNode->lt);
		Output(MyUI64TOA(lines));
		Output(MyUI64TOA(total));
		Output("Done!");
	}

	// Close files
	fclose(PerftFENsFileInput);
	fclose(PerftFENsFileOutput);
	//fclose(TEST);
}

#endif // EXPERIMENTAL
