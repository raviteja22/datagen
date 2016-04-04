#include <fstream>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std;

class ColumnDataGenerator {
   public:
      virtual string str() const = 0;
      virtual string getNext() = 0;
};

class ColumnDataGeneratorSequence : public ColumnDataGenerator {
   private:
      int seed_;
   public:
      ColumnDataGeneratorSequence(int);
      string str() const;
      string getNext();
};

ColumnDataGeneratorSequence::ColumnDataGeneratorSequence(int s) :
   seed_(s)
{}

string ColumnDataGeneratorSequence::str() const {
   return "sequence";
}

string ColumnDataGeneratorSequence::getNext() {
   return to_string(seed_++);
}

class ColumnDataGeneratorText : public ColumnDataGenerator {
   private:
      string length_;
      size_t lengthLimit_;
   public:
      ColumnDataGeneratorText(string&&, size_t);
      string str() const;
      string getNext();
};

ColumnDataGeneratorText::ColumnDataGeneratorText(string&& l, size_t ll) :
   length_(l),
   lengthLimit_(ll)
{
   ::srand(static_cast<unsigned int>(time(0))); // Fresh randomness seed
}

string ColumnDataGeneratorText::str() const {
   return "random-text";
}

string ColumnDataGeneratorText::getNext() {
   size_t len = ( length_.compare("variable") ? lengthLimit_ : (rand() % lengthLimit_) );
   string res(len, 0);
   static const char alphanum[] =
      " "
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

   for (int i = 0; i < len; ++i) {
      res[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
   }
   return res;
   //todo: Can this return some real english words?
}

class ColumnDef {
   private:
      string name_;
      string type_;
      unique_ptr<ColumnDataGenerator> dataGenerator_;
   public:
      ColumnDef(const string&& name, const string&& type);
      string str(); 
      void addDataGenerator(ColumnDataGenerator *);
      string getNext();
      string name();
};

ColumnDef::ColumnDef(const string&& name, const string&& type) :
   name_(name),
   type_(type)
{}

string ColumnDef::str() { 
   return name_ + "(" + type_ + ")" 
      + ( dataGenerator_ ? "<" + dataGenerator_->str() + ">" : "" ); 
}

string ColumnDef::getNext() {
   return dataGenerator_ ? dataGenerator_->getNext() : "";
}

string ColumnDef::name() {
   return name_;
}

void ColumnDef::addDataGenerator(ColumnDataGenerator *cdg) {
   dataGenerator_ = move(unique_ptr<ColumnDataGenerator>(cdg));
}

class TableDef {
   private:
      vector<unique_ptr<ColumnDef>> columns_;
      string delimiter_ = ",";
   public:
      void addColumn(ColumnDef*);
      void delimiterIs(const string&&);
      string str();
      string getHeader();
      string getNextRow();
};

void TableDef::addColumn(ColumnDef* col) {
   columns_.push_back(move(unique_ptr<ColumnDef>(col)));
}

void TableDef::delimiterIs(const string&& d) {
   delimiter_ = d;
}

string TableDef::getHeader() {
   string res;
   for(auto const& c: columns_) {
      if( &c != &columns_[0] ) { res += delimiter_; }
      res += c->name();
   }
   return res;
}

string TableDef::getNextRow() {
   string res;
   for(auto const& c: columns_) {
      if( &c != &columns_[0] ) { res += delimiter_; }
      res += c->getNext();
   }
   return res;
}

string TableDef::str() {
   string res;
   for(auto const& c:columns_) {
      res += "{" + c->str() + "}\n";
   }
   return res;
}

int main(int argc, char** argv) {
   try {
      if(argc < 2) {
         cerr << "Usage: datagen <format-file>" << endl;
         exit(0);
      }
      ifstream fin(argv[1]);
      stringstream buf;
      buf << fin.rdbuf();
      fin.close();
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(buf, pt);

      int countRows = stoi(pt.get_child("rows").data());
      
      if(countRows > 0) {
         unique_ptr<TableDef> tp(new TableDef());

         tp->delimiterIs(static_cast<string>(pt.get_child("delimiter").data()));

         for(auto const& c: pt.get_child("columns")) {
            ColumnDef* col = new ColumnDef(static_cast<string>(c.second.get_child("name").data()),
                                     static_cast<string>(c.second.get_child("type").data()));
            if(!c.second.get_child("data.generator").data().compare("random-text")) {
               col->addDataGenerator(
                  new ColumnDataGeneratorText(static_cast<string>(c.second.get_child("data.length").data()),
                                                stoi(c.second.get_child("data.length-limit").data()))
                  );
            } else if(!c.second.get_child("data.generator").data().compare("sequence")) {
               col->addDataGenerator(
                  new ColumnDataGeneratorSequence(stoi(c.second.get_child("data.seed").data()))
                  );
            }
            tp->addColumn(col);
         }
         cout << tp->getHeader() << endl;
         for(int i = 0; i < countRows; i++) {
            cout << tp->getNextRow() << endl;
         }
      }
   } catch( std::exception const& e) {
      cerr << "Exception : " << e.what() << endl;
   }
}
