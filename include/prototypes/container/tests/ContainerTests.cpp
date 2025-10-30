#include <gtest/gtest.h>

#include <type_traits>

#include "Prototypes.hpp"

// Фикстура с общим буфером и алиасами полей
class ContainerSuite : public testing::Test {
 protected:
  static inline uint8_t buffer_[100] = {'a', 'b', 'c', 'd', 'e', 'f',
                                        'g', 'h', 'i', 'j', 'k'};
  using SimpleFields = proto::test::SympleFields<buffer_>;
  using ComplexFieldsT = proto::test::ComplexFields<buffer_>;
  using SimpleTuple = SimpleFields::proto_fields;
  using ComplexTuple = ComplexFieldsT::proto_fields;
};

TEST_F(ContainerSuite, IndexingByNumber) {
  proto::FieldContainer<SimpleTuple> container;

  // если в FieldContainer объявлен static constexpr size
  // раскомментируй следующую строку:
  // static_assert(decltype(container)::size == 5, "Container must contain 5
  // fields");

  auto& id = container.get<0>();
  auto& len = container.get<1>();
  auto& alen = container.get<2>();
  auto& data = container.get<3>();
  auto& crc = container.get<4>();

  EXPECT_EQ(id.NAME, proto::FieldName::ID_FIELD);
  EXPECT_EQ(len.NAME, proto::FieldName::LEN_FIELD);
  EXPECT_EQ(alen.NAME, proto::FieldName::ALEN_FIELD);
  EXPECT_EQ(data.NAME, proto::FieldName::DATA_FIELD);
  EXPECT_EQ(crc.NAME, proto::FieldName::CRC_FIELD);
}

TEST_F(ContainerSuite, IndexingByName) {
  proto::FieldContainer<SimpleTuple> container;

  auto& id = container.get<proto::FieldName::ID_FIELD>();
  auto& len = container.get<proto::FieldName::LEN_FIELD>();
  auto& alen = container.get<proto::FieldName::ALEN_FIELD>();
  auto& data = container.get<proto::FieldName::DATA_FIELD>();
  auto& crc = container.get<proto::FieldName::CRC_FIELD>();

  EXPECT_EQ(id.NAME, proto::FieldName::ID_FIELD);
  EXPECT_EQ(len.NAME, proto::FieldName::LEN_FIELD);
  EXPECT_EQ(alen.NAME, proto::FieldName::ALEN_FIELD);
  EXPECT_EQ(data.NAME, proto::FieldName::DATA_FIELD);
  EXPECT_EQ(crc.NAME, proto::FieldName::CRC_FIELD);
}

TEST_F(ContainerSuite, NameAndIndexAccessAreSameObjects) {
  proto::FieldContainer<SimpleTuple> container;

  auto& id_by_name = container.get<proto::FieldName::ID_FIELD>();
  auto& len_by_name = container.get<proto::FieldName::LEN_FIELD>();
  auto& alen_by_name = container.get<proto::FieldName::ALEN_FIELD>();
  auto& data_by_name = container.get<proto::FieldName::DATA_FIELD>();
  auto& crc_by_name = container.get<proto::FieldName::CRC_FIELD>();

  auto& id_by_idx = container.get<0>();
  auto& len_by_idx = container.get<1>();
  auto& alen_by_idx = container.get<2>();
  auto& data_by_idx = container.get<3>();
  auto& crc_by_idx = container.get<4>();

  EXPECT_EQ(&id_by_name, &id_by_idx);
  EXPECT_EQ(&len_by_name, &len_by_idx);
  EXPECT_EQ(&alen_by_name, &alen_by_idx);
  EXPECT_EQ(&data_by_name, &data_by_idx);
  EXPECT_EQ(&crc_by_name, &crc_by_idx);
}

TEST_F(ContainerSuite, ForEachTypeOrderAndNames) {
  proto::FieldContainer<SimpleTuple> container;

  int i = 0;
  container.for_each_type([&](auto& field) {
    switch (i) {
      case 0:
        EXPECT_EQ(field.NAME, proto::FieldName::ID_FIELD);
        break;
      case 1:
        EXPECT_EQ(field.NAME, proto::FieldName::LEN_FIELD);
        break;
      case 2:
        EXPECT_EQ(field.NAME, proto::FieldName::ALEN_FIELD);
        break;
      case 3:
        EXPECT_EQ(field.NAME, proto::FieldName::DATA_FIELD);
        break;
      case 4:
        EXPECT_EQ(field.NAME, proto::FieldName::CRC_FIELD);
        break;
      default:
        FAIL() << "Unexpected field index: " << i;
    }
    ++i;
  });
  EXPECT_EQ(i, 5);
}

TEST_F(ContainerSuite, DataTypesOfGetData) {
  proto::FieldContainer<SimpleTuple> container;

  auto id_ptr = container.get<proto::FieldName::ID_FIELD>().get_ptr();
  auto len_val = *container.get<proto::FieldName::LEN_FIELD>().get_ptr();
  auto alen_val = *container.get<proto::FieldName::ALEN_FIELD>().get_ptr();
  auto data_ptr = container.get<proto::FieldName::DATA_FIELD>().get_ptr();
  auto crc_val = *container.get<proto::FieldName::CRC_FIELD>().get_ptr();

  static_assert(std::is_same_v<decltype(id_ptr), const uint8_t*>,
                "ID_FIELD must be const uint8_t*");
  static_assert(std::is_same_v<decltype(len_val), uint8_t>,
                "LEN_FIELD must be uint8_t");
  static_assert(std::is_same_v<decltype(alen_val), uint8_t>,
                "ALEN_FIELD must be uint8_t");
  static_assert(
      std::is_same_v<decltype(data_ptr), const proto::test::dataType*>,
      "DATA_FIELD must be const dataType*");
  static_assert(std::is_same_v<decltype(crc_val), uint16_t>,
                "CRC_FIELD must be uint16_t");

  // лёгкие рантайм-проверки, чтобы компилятор не выкидывал переменные
  (void)id_ptr;
  (void)data_ptr;
  EXPECT_EQ(len_val | 0, len_val);
  EXPECT_EQ(alen_val | 0, alen_val);
  EXPECT_EQ(crc_val | 0, crc_val);
}