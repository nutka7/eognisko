#include <algorithm>
#include <cstdint>
#include <string.h>
#include <assert.h>
#include "mixer.h"


void mixer(struct mixer_input* inputs, size_t n,
           void* output_buf, size_t* output_size,
           unsigned long tx_interval_ms)
{
    using std::min;
    using std::max;

    size_t wanted_2bytes = 176 * (size_t) tx_interval_ms / 2;
    size_t avaiable_2bytes = *output_size / 2;
    size_t result_size = min(wanted_2bytes, avaiable_2bytes); 
    
    *output_size = 2 * result_size;

    memset(output_buf, 0, 2 * result_size);

    int16_t* result_data = (int16_t*) output_buf;

    for (size_t i = 0; i < n; ++i) 
    {
        int16_t* input_data = (int16_t*) inputs[i].data;
        size_t input_size = inputs[i].len / 2;
        size_t input_used = min(input_size, result_size);
        
        inputs[i].consumed = 2 * input_used;
        
        for (size_t j = 0; j < input_used; ++j) {
            int32_t sum = (int32_t) result_data[j] + (int32_t) input_data[j];
            result_data[j] = min(INT16_MAX, max(INT16_MIN, sum));
        }
    }
}

/*
int16_t data0[] = {1,2, 46, 21, INT16_MAX - 100, INT16_MIN + 1};

int16_t data1[] = {-2,-3, -7, 12, INT16_MAX - 200, -2, 777};

int16_t result[] = {-1, -1, 39, 33, INT16_MAX, INT16_MIN, 777, 0};

struct mixer_input inputs[] = {
    {(void*) data0, sizeof(data0), 0},
    {(void*) data1, sizeof(data1), 0},
};

int8_t output_buf[2000];
size_t output_size = 2000;

int main() {
    mixer(inputs, 2, output_buf, &output_size, 3);
    assert (output_size == 176 * 3);
    assert (inputs[0].consumed == inputs[0].len);
    assert (inputs[1].consumed == inputs[1].len);

    int8_t* raw_result = (int8_t*) result;
    
    for (int i = 0; i < sizeof(result); ++i) {
        assert (raw_result[i] == output_buf[i]);
    }
    
    for (int i = sizeof(result); i < 176 * 3; ++i) {
        assert (0 == output_buf[i]);
    }

    
    output_size = 9;
    mixer(inputs, 2, output_buf, &output_size, 5);
    assert (output_size == 8);
    assert (inputs[0].consumed == 8);
    assert (inputs[1].consumed == 8);

    for (int i = 0; i < 8; ++i) {
        assert (raw_result[i] == output_buf[i]);
    }

    return 0;
}
*/
