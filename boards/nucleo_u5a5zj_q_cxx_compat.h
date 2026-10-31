/*
 * Force-included into C++ for the NUCLEO-U5A5ZJ-Q trial. Zephyr headers pull the
 * STM32Cube CMSIS/HAL headers into every F Prime TU, and their bare macros
 * (CRC, HASH_DIGEST_LENGTH(), ...) collide with F Prime identifiers. Include the
 * SoC header once up front, then drop the colliding names.
 */
#include <soc.h>
#undef CRC
#undef HASH_DIGEST_LENGTH
