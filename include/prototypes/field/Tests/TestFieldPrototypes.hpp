#pragma once

#include "FieldPrototype.hpp"
#include "DataField.hpp"


namespace proto{

    namespace test{


        template<
                FieldName NAME,
                typename T,
                uint8_t* BASE,
                FieldFlags FLAGS,
                size_t MAX_SIZE =4096,
                size_t SIZE = (std::is_pointer_v<T> ? kAnySize : sizeof(T)),
                typename std::conditional_t<std::is_pointer_v<T>, std::remove_pointer_t<T>, T>* CONST_VALUE = nullptr,
                MatcherType MATCHER = nullptr>
        class TestFieldPrototype : public proto::FieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE, CONST_VALUE, MATCHER> {
        public:
            using delegate = proto::MatchStatus (*)(void*);
            delegate TestMatcher(){
                return this->matcher_;
            }
            static const size_t size{SIZE};
            size_t TestOffset() {;
                return this->offset_;
            }
            uint8_t* TestBase() {;
                return this->base_;
            }
            uint8_t* TestGetRaw() {;
                return this->GetRaw();
            }
            size_t TemplateSize(){
                return this->size_;
            };
            void TestSetOffset(size_t offset) {
                this->SetOffset(offset);
            }
            void TestSet(const T &value) {
                this->Set(value);
            }
            void TestApplyConst() {
                this->ApplyConst();
            }
            [[nodiscard]] auto* TestGet() {
                return this->Value();
            }
            [[nodiscard]] const T* TestGet() const {
                return this->Get();
            }
        };

        template<typename PACKETS, uint8_t* BASE, FieldFlags FLAGS, size_t MAX_SIZE = 4096>
        class TestDataFieldPrototype : public proto::DataFieldPrototype<PACKETS, BASE, FLAGS, MAX_SIZE> {
        public:
            using delegate = proto::MatchStatus (*)(void*);
            delegate TestMatcher(){
                return this->matcher_;
            }
            size_t TemplateSize(){
                return this->size_;
            };
            size_t TemplateOffset() {;
                return this->offset_;
            }
            void TestSetOffset(size_t offset) {
                this->SetOffset(offset);
            }
            void TestSet(uint8_t *value) {
                this->Set(value);
            }
            [[nodiscard]] uint8_t* TestGet() {
                return this->Get();
            }
            [[nodiscard]] const uint8_t* TestGet() const {
                return this->Get();
            }
        };
    }

    template<FieldName NAME, typename T, uint8_t* BASE, FieldFlags FLAGS, size_t MAX_SIZE, size_t SIZE, auto* CONST_VALUE, MatcherType MATCHER>
    struct fieldsTuple<test::TestFieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE, CONST_VALUE, MATCHER>> {
        using Type = test::TestFieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE, CONST_VALUE, MATCHER>;
    };

    template< typename T, uint8_t* BASE, FieldFlags FLAGS, size_t MAX_SIZE>
    struct fieldsTuple<test::TestDataFieldPrototype<T, BASE, FLAGS, MAX_SIZE>> {
        using Type = test::TestDataFieldPrototype<T, BASE, FLAGS, MAX_SIZE>;
    };
}