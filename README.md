# A Library for Decoding RDS/RBDS data.

## Overview

This project aims to provide complete decode implementation of the
[RDS/RBDS](https://en.wikipedia.org/wiki/Radio_Data_System) protocol
as defined by the
[RBDS Specification](http://www.interactive-radio-system.com/docs/rbds1998.pdf).

This library has a single implementation, but with two public facing API's.
[Mongoose OS](https://mongoose-os.com/) is supported, but all functions
are prefixed with `mgos_` to conform with naming conventions for that
platform.

Contributions are welcome, please see [CONTRIBUTIONS.md](CONTRIBUTIONS.md)
for more information.

## Building

On Mongoose OS add the following library dependency to the application
`mos.yml` file:

```yaml
libs:
  - origin: https://github.com/cmumford/rds
```

On all other platforms use cmake as follows:

```sh
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
```

## Example Use

```c
struct rds_data data;

// Create the RDS decoder.
const struct rds_decoder_config config = {
    .advanced_ps_decoding = true,
    .rds_data = &data,
};
struct rds_decoder* decoder = rds_decoder_create(&config);

// Populate this structure with RDS blocks and (if available)
// block error values from the receiving device.
const struct rds_blocks blocks = {
    {block_a, BLER_NONE },
    {block_b, BLER_NONE },
    {block_c, BLER_NONE },
    {block_d, BLER_NONE }};
rds_decoder_decode(decoder, &blocks);

// Use one of the decoded RDS data values.
if (data.valid_values & RDS_PTY) {
  // Do something with a decoded RDS value.
  printf("PTY:  %u\n", rds_data.pty);
}

// When no longer used, delete the RDS decoder.
rds_decoder_delete(decoder);
```

Mongoose OS is nearly identical, but with `mgos_` prefixes:

```c
struct rds_data data;

// Create the RDS decoder.
const struct rds_decoder_config config = {
    .advanced_ps_decoding = true,
    .rds_data = &data,
};
struct rds_decoder* decoder = mgos_rds_decoder_create(&config);

// Populate this structure with RDS blocks and (if available)
// block error values from the receiving device.
const struct rds_blocks blocks = {
    {block_a, BLER_NONE },
    {block_b, BLER_NONE },
    {block_c, BLER_NONE },
    {block_d, BLER_NONE }};
mgos_rds_decoder_decode(decoder, &blocks);

// Use one of the decoded RDS data values.
if (data.valid_values & RDS_PTY) {
  // Do something with a decoded RDS value.
  printf("PTY:  %u\n", rds_data.pty);
}

// When no longer used, delete the RDS decoder.
mgos_rds_decoder_delete(decoder);
```
