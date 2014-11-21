function abiNameToDirName(abiName)
{
    if (abiName.startsWith("armeabi"))
        return "arm";
    if (abiName.startsWith("arm64"))
        return "arm64";
    return abiName;
}
