#ifndef __KITE_FILE_HEADERS_H__
#define __KITE_FILE_HEADERS_H__

#define KSB_FILE_MAGIC 0x1B10

#include <stdint.h>

struct File_binary_header_t {
  uint32_t magic;
  uint16_t K;
  bool collecting_origins;
  uint8_t resvd;
  uint64_t genome_count;
  uint64_t single_genome_id; /* if its a single genome, we hold the id here so
                                we don't need per-kmer origin vector */
  uint64_t kmer_count;
  uint64_t kmer_data_offset; /* offset to compressed data, 0 indicates no
                                compression */
} __attribute__((packed));

struct File_binary_kmer_t {
  uint32_t genome_vector_len;
  uint32_t close_to_vector_len;
  uint64_t n_genomes;
  uint64_t counter;
  unsigned char data[0]; /* kmer in 4bpb form | genome vector 64b uints |
                   close_to_vector 64b uints */
} __attribute__((packed));

typedef struct {
  unsigned high : 4;
  unsigned low : 4;
} __attribute__((packed)) base_4bit_t;

inline unsigned char byte_to_symbol(unsigned b) {
  /* must correspond to bitstring below */
  switch (b) {
    case 0b001:
      return 'A';
    case 0b010:
      return 'C';
    case 0b100:
      return 'G';
    case 0b111:
      return 'T';
    case 0b101:
      return 'N';
    case 0b011:
      return 'N';
    case 0b110:
      return 'E'; /* error marker */
    case 0b000:
      return 0;
    default:
      throw General_exception("byte_to_symbol conversion failed (%x - >%c<)", b,
                              b);
  }
}

inline std::string str(base_4bit_t* data, unsigned k) {
  std::stringstream ss;

  for (unsigned i = 0; i < k / 2; i++) {
    ss << byte_to_symbol(data[i].low) << byte_to_symbol(data[i].high);
  }

  return ss.str();
}

#endif