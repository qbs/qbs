class Car implements Vehicle
{
    public void go()
    {
        System.out.println("Driving!");
    }

    public class InternalCombustionEngine
    {
        public native void run();
    }
}
