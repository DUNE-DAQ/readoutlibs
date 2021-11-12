/**
 * @file readoutlibs_BufferedReadWrite_test.cxx Unit Tests for BufferedFileWriter and BufferedFileReader
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

/**
 * @brief Name of this test module
 */
#define BOOST_TEST_MODULE readoutlibs_BufferedReadWrite_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "logging/Logging.hpp"
#include "readoutlibs/utils/BufferedFileReader.hpp"
#include "readoutlibs/utils/BufferedFileWriter.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dunedaq::readoutlibs;

BOOST_AUTO_TEST_SUITE(readoutlibs_BufferedReadWrite_test)

void
test_read_write(BufferedFileWriter<>& writer, BufferedFileReader<int>& reader, uint numbers_to_write)
{
  std::vector<int> numbers(numbers_to_write / sizeof(int));

  bool write_successful = false;
  for (uint i = 0; i < numbers.size(); ++i) {
    numbers[i] = i;
    write_successful = writer.write(reinterpret_cast<char*>(&i), sizeof(i));
    BOOST_REQUIRE(write_successful);
  }

  writer.close();

  int read_value;
  bool read_successful = false;
  for (uint i = 0; i < numbers.size(); ++i) {
    read_successful = reader.read(read_value);
    if (!read_successful)
      TLOG() << i << std::endl;
    BOOST_REQUIRE(read_successful);
    BOOST_REQUIRE_EQUAL(read_value, numbers[i]);
  }

  read_successful = reader.read(read_value);
  BOOST_REQUIRE(!read_successful);

  reader.close();

  remove("test.out");
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_one_int)
{
  TLOG() << "Trying to write and read one int" << std::endl;
  remove("test.out");
  BufferedFileWriter writer;
  writer.open("test.out", 4096);
  BufferedFileReader<int> reader;
  reader.open("test.out", 4096);
  uint numbers_to_write = 1;

  test_read_write(writer, reader, numbers_to_write);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_extended)
{
  TLOG() << "Trying to read and write more ints" << std::endl;
  remove("test.out");
  BufferedFileWriter writer;
  writer.open("test.out", 4096);
  BufferedFileReader<int> reader;
  reader.open("test.out", 4096);
  uint numbers_to_write = 4096 * 4096;

  test_read_write(writer, reader, numbers_to_write);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_zstd)
{
  TLOG() << "Testing zstd compression" << std::endl;
  remove("test.out");
  BufferedFileWriter writer;
  writer.open("test.out", 4096, "zstd");
  BufferedFileReader<int> reader;
  reader.open("test.out", 4096, "zstd");
  uint numbers_to_write = 4096 * 4096;

  test_read_write(writer, reader, numbers_to_write);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_lzma)
{
  TLOG() << "Testing lzma compression" << std::endl;
  remove("test.out");
  BufferedFileWriter writer;
  writer.open("test.out", 4096, "lzma");
  BufferedFileReader<int> reader;
  reader.open("test.out", 4096, "lzma");
  uint numbers_to_write = 4096 * 4096;

  test_read_write(writer, reader, numbers_to_write);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_zlib)
{
  TLOG() << "Testing zlib compression" << std::endl;
  remove("test.out");
  BufferedFileWriter writer;
  writer.open("test.out", 4096, "zlib");
  BufferedFileReader<int> reader;
  reader.open("test.out", 4096, "zlib");
  uint numbers_to_write = 4096 * 4096;

  test_read_write(writer, reader, numbers_to_write);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_not_opened)
{
  TLOG() << "Try to read and write on uninitialized instances" << std::endl;
  BufferedFileWriter writer;
  int number = 42;
  bool write_successful = writer.write(reinterpret_cast<char*>(&number), sizeof(number));
  BOOST_REQUIRE(!write_successful);

  BufferedFileReader<int> reader;
  int value;
  bool read_successful = reader.read(value);
  BOOST_REQUIRE(!read_successful);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_already_closed)
{
  TLOG() << "Try to write on closed writer" << std::endl;
  BufferedFileWriter writer("test.out", 4096);
  writer.close();
  int number = 42;
  bool write_successful = writer.write(reinterpret_cast<char*>(&number), sizeof(number));
  BOOST_REQUIRE(!write_successful);
}

BOOST_AUTO_TEST_CASE(BufferedReadWrite_destructor)
{
  TLOG() << "Testing automatic closing on destruction" << std::endl;
  remove("test.out");
  {
    BufferedFileWriter writer("test.out", 4096);
    int number = 42;
    bool write_successful = writer.write(reinterpret_cast<char*>(&number), sizeof(number));
    BOOST_REQUIRE(write_successful);
  }
  BufferedFileReader<int> reader("test.out", 4096);
  int value;
  bool read_successful = reader.read(value);
  BOOST_REQUIRE(read_successful);
  BOOST_REQUIRE_EQUAL(value, 42);

  reader.close();

  remove("test.out");
}

BOOST_AUTO_TEST_SUITE_END()
