function getName(qbsModule)
{
    if (qbsModule.debugInformation)
        return "MyProduct_debug";
    else
        return "MyProduct";
}

function getInstallDir()
{
    return "somewhere";
}
