import java.util.ArrayList;
import glob.RandomStuff;

class Vehicles
{
    public static void main(String[] args)
    {
        System.loadLibrary("native");
        RandomStuff.bar();
        ArrayList<Vehicle> vehicles = new ArrayList<Vehicle>();

        for (int i = 0; i < 3; i++)
        {
            vehicles.add(new Car());
        }

        for (int i = 0; i < 3; i++)
        {
            vehicles.add(new Jet());
        }

        for (int i = 0; i < 4; i++)
        {
            Ship ship = new Ship();
            ship.isInSpace = i % 2 == 0;
            vehicles.add(ship);
        }

        for (int i = 0; i < vehicles.size(); i++)
        {
            vehicles.get(i).go();
        }

        // doesn't compile, must be a bug
        // delete vehicles;
    }
}
