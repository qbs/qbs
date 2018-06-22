import qbs
import qbs.Process

Module {
    Depends { name: "lex_yacc" }
    Probe {
        id: bisonProbe
        property string yaccBinary: lex_yacc.yaccBinary
        configure: {
            var p = Process();
            found = p.exec(yaccBinary, ["-V"]) == 0 && p.readStdOut().contains("bison");
            p.close();
        }
    }
    Properties {
        condition: bisonProbe.found
        lex_yacc.yaccFlags: "-y"
    }
}
