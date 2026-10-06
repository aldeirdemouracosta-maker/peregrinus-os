#pragma once
// Freestanding float math for the local-LLM engine (no libm in the kernel). The same object is
// linked into the host reference build of llama2.c's run.c, so kernel and host compute
// bit-identical results. Accuracy: a few ULP over the ranges the transformer uses.
extern "C" {
float pg_sqrtf(float x);
float pg_expf(float x);
float pg_logf(float x);
float pg_powf(float base, float exponent);
float pg_sinf(float x);
float pg_cosf(float x);
}
