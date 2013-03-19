function rfc1034(inStr)
{
    return inStr.replace(/[^-A-Za-z0-9.]/g,'-');
}
