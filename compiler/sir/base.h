#ifndef SEQ_IR_BASE_H
#define SEQ_IR_BASE_H

#include <map>
#include <string>

namespace seq {
    namespace ir {

        static const std::string kNameAttributeKey = "nameAttribute";

        class Attribute {
        public:
            virtual std::string textRepresentation() const = 0;
        };

        class StringAttribute : public Attribute {
        private:
            std::string value;

        public:
            explicit StringAttribute(std::string value);

            std::string textRepresentation() const override;
        };

        class BoolAttribute : public Attribute {
        private:
            bool value;

        public:
            explicit BoolAttribute(bool value);

            std::string textRepresentation() const override;
        };

        class AttributeHolder {
        private:
            std::map<std::string, std::shared_ptr<Attribute>> kvStore;

        public:
            AttributeHolder();

            virtual std::string textRepresentation() const;

            void setAttribute(std::string key, std::shared_ptr<Attribute> value);
            std::shared_ptr<Attribute> getAttribute(std::string key) const;
        };
    }
}

#endif //SEQ_IR_BASE_H
