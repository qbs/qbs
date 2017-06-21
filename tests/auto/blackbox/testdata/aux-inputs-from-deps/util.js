function sleep(timeInMs)
{
    var referenceTime = new Date();
    var time = null;
    do {
        time = new Date();
    } while (time - referenceTime < timeInMs);
}
