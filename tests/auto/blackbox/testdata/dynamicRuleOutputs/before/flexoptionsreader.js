/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the examples of Qbs.
**
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
****************************************************************************/

// needs import qbs.TextFile

function readFlexOptions(filePath)
{
    function splitOptions(str)
    {
        var options = [];
        var opt = "";
        var inquote = false;
        for (var i = 0; i < str.length; ++i) {
            if (str[i] === '"') {
                opt += '"';
                inquote = !inquote;
            } else if (str[i] === ' ' && !inquote) {
                options.push(opt);
                opt = "";
            } else {
                opt += str[i];
            }
        }
        if (opt.length)
            options.push(opt);
        return options;
    }

    function unquote(str)
    {
        var l = str.length;
        if (l > 2 && str[0] === '"' && str[l - 1] === '"')
            return str.substr(1, l - 2);
        return str;
    }

    function parseOptionLine(result, str)
    {
        var options = splitOptions(str);
        var re = /^(outfile|header-file)=(.*)$/;
        var reres;
        for (var k in options) {
            re.lastIndex = 0;
            reres = re.exec(options[k]);
            if (reres === null)
                continue;
            result[reres[1]] = unquote(reres[2]);
        }
    }

    var tf = new TextFile(input.filePath);
    var line;
    var optrex = /^%option\s+(.*$)/;
    var res;
    var options = {};
    while (!tf.atEof()) {
        line = tf.readLine();
        if (line === "%%")
            break;
        optrex.lastIndex = 0;
        res = optrex.exec(line);
        if (res === null)
            continue;
        parseOptionLine(options, res[1]);
    }
    tf.close();
    return options;
}

