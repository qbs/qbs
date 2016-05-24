class Car8 implements Vehicle
{
    private InternalCombustionEngine engine;

    public Car8() {
        engine = new InternalCombustionEngine();
    }

    public void go()
    {
        System.out.println("Driving!");
        engine.run();
    }

    public class InternalCombustionEngine
    {
        public native void run();

        public class ChemicalReaction {
            public native void occur();

            public class Atoms {
                @java.lang.annotation.Native
                public int hydrogenAtomCount;
            }
        }
    }
}
