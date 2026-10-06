/* Routes run.c's libm calls to the kernel's math (kernel/llm/llm_math.cpp), so the reference
 * and the kernel engine evaluate exactly the same functions. Included after run.c's headers. */
float pg_sqrtf(float x); float pg_expf(float x); float pg_powf(float b, float e); float pg_sinf(float x); float pg_cosf(float x);
#define sqrtf pg_sqrtf
#define expf pg_expf
#define powf pg_powf
#define sinf pg_sinf
#define cosf pg_cosf
