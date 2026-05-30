#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

// 1. Definition unserer Token-Typen
enum class TokenType {
    KeywordClass, KeywordInterface,
    KeywordAbstract, KeywordStatic,
    Identifier,
    Plus, Minus, Hash,           // Sichtbarkeiten (+, -, #)
    Colon, Semicolon, Comma,     // Operatoren und Trennzeichen
    OpenBrace, CloseBrace,       // { }
    OpenParen, CloseParen,       // ( )
    Less, Greater,
    Star, Ampersand, Arrow,
    OpenBracket, CloseBracket, StringLiteral,
    EndOfFile, Unknown
};

// 2. Die Token-Struktur
struct Token {
    TokenType type;
    std::string_view text;
    size_t line;
};

// Sichtbarkeit für Methoden und Felder
enum class Visibility { Public, Private, Protected, None };

// Hilfsfunktion zur Übersetzung der Sichtbarkeit
std::string visToString(Visibility v) {
    switch (v) {
        case Visibility::Public: return "+";
        case Visibility::Private: return "-";
        case Visibility::Protected: return "#";
        default: return "";
    }
}

enum class RelationType { Association, Aggregation, Composition, Undirected }; // Undirected hinzugefügt

struct Relation {
    RelationType type;
    std::string targetClass;
    std::string sourceMultiplicity; // NEU
    std::string targetMultiplicity; // Umbenannt von 'multiplicity'

    std::string toPlantUML(const std::string& sourceClass) const {
        std::stringstream ss;
        ss << sourceClass << " ";
        
        // Optionale Source-Multiplizität (links vom Pfeil)
        if (!sourceMultiplicity.empty()) {
            ss << "\"" << sourceMultiplicity << "\" ";
        }
        
        // Die Art des Pfeils
        switch(type) {
            case RelationType::Association: ss << "-->"; break;
            case RelationType::Undirected:  ss << "--"; break;  // NEU
            case RelationType::Aggregation: ss << "o--"; break;
            case RelationType::Composition: ss << "*--"; break;
        }
        
        // Optionale Target-Multiplizität (rechts vom Pfeil)
        if (!targetMultiplicity.empty()) {
            ss << " \"" << targetMultiplicity << "\"";
        }
        
        ss << " " << targetClass;
        return ss.str();
    }
};

// Repräsentiert ein Feld ODER eine Methode
struct Member {
    Visibility visibility = Visibility::None;
    bool isStatic = false;
    bool isAbstract = false;
    bool isConstructor = false;
    std::string type;
    std::string name;
    bool isMethod = false;
    
    // NEU: Nimmt das Theme-Flag entgegen
    std::string toPlantUML(bool useClassic) const {
        std::stringstream ss;
        ss << "  ";
        
        if (isStatic) ss << "{static} ";
        
        if (isAbstract) {
            ss << "{abstract} ";
            // Schreibt <<abstract>> nur im Classic-Theme explizit aus
            if (useClassic) ss << "<<abstract>> "; 
        }
        
        ss << visToString(visibility);
        
        if (isConstructor) {
            ss << "<<create>> " << name << "()";
        } else {
            ss << name << (isMethod ? "()" : "") << ": " << type;
        }
        return ss.str();
    }
};

// Repräsentiert eine Klasse ODER ein Interface
struct Entity {
    bool isInterface = false;
    bool isAbstract = false;
    std::string name;
    std::vector<std::string> parents;
    std::vector<Member> members;
    std::vector<Relation> relations;

    // NEU: Nimmt das Theme-Flag entgegen
    std::string toPlantUML(bool useClassic) const {
        std::stringstream ss;
        std::string entityType = "class ";
        
        if (isInterface) entityType = "interface ";
        else if (isAbstract) entityType = "abstract class ";
        
        ss << entityType << name;
        
        // Schreibt <<abstract>> für Klassen nur im Classic-Theme aus
        if (isAbstract && !isInterface && useClassic) {
            ss << " <<abstract>>";
        }
        ss << " {\n";
        
        for (const auto& m : members) {
            // Flag an die Felder/Methoden durchreichen
            ss << m.toPlantUML(useClassic) << "\n";
        }
        ss << "}\n\n";

        for (const auto& parent : parents) {
            ss << parent << " <|-- " << name << "\n";
        }

        for (const auto& rel : relations) {
            // Relationen benötigen das Flag aktuell nicht
            ss << rel.toPlantUML(name) << "\n";
        }

        return ss.str();
    }
};

// Die Wurzel unseres Baumes (Das gesamte Dokument)
struct Program {
    std::vector<Entity> entities;
    bool useClassicTheme = false;

    std::string generatePlantUML() const {
        std::stringstream ss;
        ss << "@startuml\n";
        
        if (useClassicTheme) {
            ss << "skinparam classAttributeIconSize 0\n";
            ss << "hide circle\n";
            ss << "skinparam monochrome true\n";
            ss << "skinparam shadowing false\n";
        }

        for (const auto& entity : entities) {
            // NEU: Das Flag wird hier an die Klassen übergeben
            ss << entity.toPlantUML(useClassicTheme);
        }
        
        ss << "@enduml\n";
        return ss.str();
    }
};

class Parser {
private:
    const std::vector<Token>& tokens;
    size_t cursor = 0;

    // --- Hilfsfunktionen für die Navigation ---
    const Token& peek() const { return tokens[cursor]; }
    const Token& previous() const { return tokens[cursor - 1]; }
    bool isAtEnd() const { return peek().type == TokenType::EndOfFile; }

    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    // Konsumiert das Token, wenn es vom erwarteten Typ ist
    bool match(TokenType type) {
        if (check(type)) {
            cursor++;
            return true;
        }
        return false;
    }

    // Erzwingt ein bestimmtes Token, andernfalls wird eine Exception geworfen
    const Token& consume(TokenType type, const std::string& errorMessage) {
        if (check(type)) {
            cursor++;
            return previous();
        }
        throw std::runtime_error("Parser Fehler in Zeile " + std::to_string(peek().line) + ": " + errorMessage);
    }

    // --- Die eigentlichen Parsing-Regeln ---
    std::string parseType() {
        // Der Basis-Typ (z.B. "List" oder "String")
        std::string typeName = std::string(consume(TokenType::Identifier, "Erwarteter Typ").text);
        
        // Wenn ein '<' folgt, steigen wir in die Generics ab
        if (match(TokenType::Less)) {
            typeName += "<";
            
            // Erstes Generic-Argument
            typeName += parseType(); 
            
            // Weitere Argumente, durch Kommata getrennt (z.B. bei Map<K, V>)
            while (match(TokenType::Comma)) {
                typeName += ", ";
                typeName += parseType();
            }
            
            consume(TokenType::Greater, "Erwartete schließende Klammer '>' bei Generic-Typ");
            typeName += ">";
        }
        return typeName;
    }


    Member parseMember() {
        Member member;

        // 1. Optionale Sichtbarkeit (+, -, #)
        if (match(TokenType::Plus)) member.visibility = Visibility::Public;
        else if (match(TokenType::Minus)) member.visibility = Visibility::Private;
        else if (match(TokenType::Hash)) member.visibility = Visibility::Protected;

        // 2. NEU: Optionale Modifikatoren (static / abstract)
        while (true) {
            if (match(TokenType::KeywordStatic)) member.isStatic = true;
            else if (match(TokenType::KeywordAbstract)) member.isAbstract = true;
            else break; // Wenn keines von beiden kommt, brechen wir ab
        }

        // 3. Den ersten Bezeichner einlesen (könnte Typ ODER Konstruktor-Name sein)
        std::string firstPart = parseType();

        // 4. Lookahead: Kommt sofort eine Klammer?
        if (match(TokenType::OpenParen)) {
            // Treffer! Es ist ein Konstruktor.
            member.name = firstPart;
            member.type = ""; // Konstruktoren haben keinen Rückgabetyp
            member.isMethod = true;
            member.isConstructor = true;

            // Parameter bis zur schließenden Klammer überspringen (für diesen Draft)
            while (!check(TokenType::CloseParen) && !isAtEnd()) { cursor++; }
            consume(TokenType::CloseParen, "Erwartete schließende Klammer ')' beim Konstruktor");
        } 
        else {
            // Kein Konstruktor. firstPart war also der Rückgabetyp. 
            // Jetzt lesen wir den echten Namen der Variable oder Methode.
            member.type = firstPart;
            member.name = std::string(consume(TokenType::Identifier, "Erwarteter Bezeichner nach Typ").text);

            // Ist es eine Methode?
            if (match(TokenType::OpenParen)) {
                member.isMethod = true;
                while (!check(TokenType::CloseParen) && !isAtEnd()) { cursor++; }
                consume(TokenType::CloseParen, "Erwartete schließende Klammer ')' bei der Methode");
            }
        }

        // 5. Abschluss
        consume(TokenType::Semicolon, "Erwartetes Semikolon ';' am Ende des Members");
        return member;
    }

    Entity parseEntity() {
        Entity entity;
        
        if (match(TokenType::KeywordAbstract)) {
            entity.isAbstract = true;
            consume(TokenType::KeywordClass, "Erwartetes 'class' nach 'abstract'");
        } 
        else if (match(TokenType::KeywordInterface)) {
            entity.isInterface = true;
        } 
        else {
            consume(TokenType::KeywordClass, "Erwartetes 'class', 'abstract class' oder 'interface'");
        }

        entity.name = std::string(consume(TokenType::Identifier, "Erwarteter Name").text);
        // --- NEU: Parsen des gesamten Klassenkopfes ---
        while (!check(TokenType::OpenBrace) && !isAtEnd()) {
            
            // 1. Vererbung / Interfaces (:)
            if (match(TokenType::Colon)) {
                do {
                    entity.parents.push_back(std::string(consume(TokenType::Identifier, "Erwartete Elternklasse").text));
                } while (match(TokenType::Comma));
            }
            // 2. Relationen (*, &, ->, -)
            // Beachten Sie, wie elegant sich das Minus hier einreiht.
            else if (match(TokenType::Star) || match(TokenType::Ampersand) || 
                     match(TokenType::Arrow) || match(TokenType::Minus)) {
                
                TokenType op = previous().type;
                RelationType relType;
                
                if (op == TokenType::Star) relType = RelationType::Composition;
                else if (op == TokenType::Ampersand) relType = RelationType::Aggregation;
                else if (op == TokenType::Arrow) relType = RelationType::Association;
                else relType = RelationType::Undirected; // TokenType::Minus

                do {
                    Relation rel;
                    rel.type = relType;
                    
                    // NEU: Optionale Source-Multiplizität VOR dem Klassennamen
                    if (match(TokenType::OpenBracket)) {
                        rel.sourceMultiplicity = std::string(consume(TokenType::StringLiteral, "Erwartete Source-Multiplizität (z.B. \"1\")").text);
                        consume(TokenType::CloseBracket, "Erwartete schließende Klammer ']'");
                    }

                    // Die Zielklasse
                    rel.targetClass = std::string(consume(TokenType::Identifier, "Erwartete Zielklasse").text);
                    
                    // Optionale Target-Multiplizität NACH dem Klassennamen
                    if (match(TokenType::OpenBracket)) {
                        rel.targetMultiplicity = std::string(consume(TokenType::StringLiteral, "Erwartete Target-Multiplizität (z.B. \"*\")").text);
                        consume(TokenType::CloseBracket, "Erwartete schließende Klammer ']'");
                    }
                    
                    entity.relations.push_back(rel);
                } while (match(TokenType::Comma));
            } 
            else {
                throw std::runtime_error("Unerwartetes Token im Klassenkopf vor '{'");
            }
        }

        // --- Der Rest bleibt wie zuvor ---
        consume(TokenType::OpenBrace, "Erwartete '{'");
        while (!check(TokenType::CloseBrace) && !isAtEnd()) {
            entity.members.push_back(parseMember()); // Der Body parst jetzt nur noch normale Attribute/Methoden
        }
        consume(TokenType::CloseBrace, "Erwartete '}'");

        return entity;
    }

public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    Program parse() {
        Program program;
        while (!isAtEnd()) {
            program.entities.push_back(parseEntity());
        }
        return program;
    }
};

// 3. Datei-IO: Lädt den gesamten Inhalt einer Datei effizient in einen String
std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Fehler: Konnte Datei '" + filepath + "' nicht öffnen.");
    }
    
    // Springe ans Ende, lies die Größe aus, reserviere Speicher und lies alles ein
    file.seekg(0, std::ios::end);
    std::string contents;
    contents.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&contents[0], contents.size());
    
    return contents;
}

// 4. Der Lexer
class Lexer {
private:
    std::string_view source;
    size_t cursor = 0;
    size_t currentLine = 1;

    // Hilfsfunktionen für die Traversierung
    char peek() const {
        if (isAtEnd()) return '\0';
        return source[cursor];
    }

    char advance() {
        return source[cursor++];
    }

    bool isAtEnd() const {
        return cursor >= source.length();
    }

    void skipWhitespace() {
        while (!isAtEnd()) {
            char c = peek();
            if (c == ' ' || c == '\r' || c == '\t') {
                advance();
            } else if (c == '\n') {
                currentLine++;
                advance();
            } else {
                break;
            }
        }
    }

    Token makeToken(TokenType type, size_t start, size_t length) {
        return Token{type, source.substr(start, length), currentLine};
    }

    bool isAlphaOrUTF8(char c) const {
        unsigned char uc = static_cast<unsigned char>(c);
        return std::isalpha(uc) || uc == '_' || uc > 127;
    }

    bool isAlnumOrUTF8(char c) const {
        unsigned char uc = static_cast<unsigned char>(c);
        return std::isalnum(uc) || uc == '_' || uc > 127;
    }

public:
    explicit Lexer(std::string_view sourceCode) : source(sourceCode) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (!isAtEnd()) {
            skipWhitespace();
            if (isAtEnd()) break;

            size_t start = cursor;
            char c = advance();

            // Einzelne Zeichen (Operatoren, Klammern)
            switch (c) {
                case '+': tokens.push_back(makeToken(TokenType::Plus, start, 1)); continue;
                case '-': 
                    if (peek() == '>') {
                        advance();
                        tokens.push_back(makeToken(TokenType::Arrow, start, 2));
                    } else {
                        tokens.push_back(makeToken(TokenType::Minus, start, 1));
                    }
                    continue;
                case '*': tokens.push_back(makeToken(TokenType::Star, start, 1)); continue;
                case '&': tokens.push_back(makeToken(TokenType::Ampersand, start, 1)); continue;
                case '[': tokens.push_back(makeToken(TokenType::OpenBracket, start, 1)); continue;
                case ']': tokens.push_back(makeToken(TokenType::CloseBracket, start, 1)); continue;
                case '#': tokens.push_back(makeToken(TokenType::Hash, start, 1)); continue;
                case ':': tokens.push_back(makeToken(TokenType::Colon, start, 1)); continue;
                case ';': tokens.push_back(makeToken(TokenType::Semicolon, start, 1)); continue;
                case ',': tokens.push_back(makeToken(TokenType::Comma, start, 1)); continue;
                case '{': tokens.push_back(makeToken(TokenType::OpenBrace, start, 1)); continue;
                case '}': tokens.push_back(makeToken(TokenType::CloseBrace, start, 1)); continue;
                case '(': tokens.push_back(makeToken(TokenType::OpenParen, start, 1)); continue;
                case ')': tokens.push_back(makeToken(TokenType::CloseParen, start, 1)); continue;
                case '<': tokens.push_back(makeToken(TokenType::Less, start, 1)); continue;
                case '>': tokens.push_back(makeToken(TokenType::Greater, start, 1)); continue;
            }

            if (c == '"') {
                while (!isAtEnd() && peek() != '"') {
                    if (peek() == '\n') currentLine++;
                    advance();
                }
                if (isAtEnd()) throw std::runtime_error("Fehler: Unterminierter String");
                advance(); // Schließende Anführungszeichen konsumieren
                
                // Wir extrahieren den String OHNE die Anführungszeichen
                tokens.push_back(makeToken(TokenType::StringLiteral, start + 1, cursor - start - 2));
                continue;
            }

            // Bezeichner und Schlüsselwörter (Buchstaben)
            if (isAlphaOrUTF8(c)) {
                while (!isAtEnd() && isAlnumOrUTF8(peek())) {
                    advance();
                }
                
                std::string_view text = source.substr(start, cursor - start);
                TokenType type = TokenType::Identifier;
                
                // Unterscheidung zwischen Identifier und Keywords
                if (text == "class") type = TokenType::KeywordClass;
                else if (text == "interface") type = TokenType::KeywordInterface;
                else if (text == "abstract") type = TokenType::KeywordAbstract;
                else if (text == "static") type = TokenType::KeywordStatic;

                tokens.push_back(makeToken(type, start, cursor - start));
                continue;
            }

            // Unbekanntes Zeichen (Fehlerbehandlung könnte hier erweitert werden)
            tokens.push_back(makeToken(TokenType::Unknown, start, 1));
        }

        tokens.push_back(Token{TokenType::EndOfFile, "", currentLine});
        return tokens;
    }
};

int main(int argc, char* argv[]) {
    bool useClassicTheme = false;
    std::vector<std::string> paths;

    // 1. Argumente intelligent parsen (Flags herausfiltern)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--classic") {
            useClassicTheme = true;
        } else {
            paths.push_back(arg); // Alles ohne '--' werten wir als Pfad
        }
    }

    // 2. Überprüfung der übrig gebliebenen Pfad-Argumente
    if (paths.empty() || paths.size() > 2) {
        std::cerr << "Fehler: Ungültige Anzahl an Argumenten.\n";
        std::cerr << "Verwendung: " << argv[0] << " [--classic] <eingabe_datei.dsl> [ausgabe_verzeichnis]\n";
        std::cerr << "Beispiel:   " << argv[0] << " --classic src/diagram.dsl ./pdfs/\n";
        return EXIT_FAILURE; 
    }

    fs::path inputFilePath = paths[0];
    fs::path outputDir = (paths.size() == 2) ? fs::path(paths[1]) : fs::current_path();

    try {
        // Erstelle das Ausgabeverzeichnis, falls es noch nicht existiert
        if (!fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }

        // 3. Einlesen und Parsen
        std::string sourceCode = readFile(inputFilePath.string());
        
        Lexer lexer(sourceCode);
        std::vector<Token> tokens = lexer.tokenize();

        Parser parser(tokens);
        Program ast = parser.parse();

        ast.useClassicTheme = useClassicTheme;

        std::string plantUmlCode = ast.generatePlantUML();

        // 4. Dynamische Dateinamen generieren
        // .stem() extrahiert den reinen Dateinamen ohne Endung (z.B. "diagram" aus "diagram.dsl")
        std::string baseName = inputFilePath.stem().string(); 
        
        // Der / Operator ist in std::filesystem überladen und fügt automatisch den korrekten Slash ein
        fs::path tempPumlPath = outputDir / (baseName + ".puml");
        fs::path finalSvgPath = outputDir / (baseName + ".svg");

        std::ofstream outFile(tempPumlPath);
        if (!outFile) {
            throw std::runtime_error("Fehler: Konnte Datei '" + tempPumlPath.string() + "' nicht erstellen.");
        }
        outFile << plantUmlCode;
        outFile.close();

        std::cout << "AST übersetzt. Starte Rendering für '" << baseName << "'...\n";

        // 5. PlantUML Aufruf
        // Da wir die .puml Datei direkt in den Zielordner legen, generiert PlantUML 
        // das SVG automatisch exakt daneben.
        std::string command = "java -jar plantuml-gplv2-1.2026.3.jar -tsvg " + tempPumlPath.string();
        int exitCode = std::system(command.c_str());

        if (exitCode == 0) {
            std::cout << "Exzellent! Diagramm generiert unter: " << finalSvgPath.string() << "\n";
            // Optional: Temporäre PlantUML-Textdatei aufräumen
            fs::remove(tempPumlPath); 
        } else {
            std::cerr << "Kritischer Fehler bei der Ausführung von PlantUML.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Abbruch: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}