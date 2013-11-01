class Ship implements Vehicle
{
    public boolean isInSpace;

    public void go()
    {
        if (isInSpace)
            System.out.println("Flying (this is a space ship)!");
        else
            System.out.println("Sailing!");
    }
}
