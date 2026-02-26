using ASCOM.DeviceInterface;
using ASCOM.LocalServer;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace ASCOM.EQControl.Telescope.V1
{
    #region Rate class
    //
    // The Rate class implements IRate, and is used to hold values
    // for AxisRates. You do not need to change this class.
    //
    // The Guid attribute sets the CLSID for ASCOM.BrebissonV1.Rate
    // The ClassInterface/None attribute prevents an empty interface called
    // _Rate from being created and used as the [default] interface
    //
    [Guid("89d51fdb-4514-4ed3-8f53-4fcd8a74d7ab")]
    [ClassInterface(ClassInterfaceType.None)]
    [ComVisible(true)]
    public class Rate : IRate
    {
        private double maximum = 0;
        private double minimum = 0;

        //
        // Default constructor - Internal prevents public creation
        // of instances. These are values for AxisRates.
        //
        internal Rate(double minimum, double maximum)
        {
            this.maximum = maximum;
            this.minimum = minimum;
        }

        #region Implementation of IRate

        public void Dispose()
        {
            // TODO Add any required object clean-up here
        }

        public double Maximum
        {
            get { return this.maximum; }
            set { this.maximum = value; }
        }

        public double Minimum
        {
            get { return this.minimum; }
            set { this.minimum = value; }
        }

        #endregion
    }
    #endregion

    #region AxisRates
    //
    // AxisRates is a strongly-typed collection that must be enumerable by
    // both COM and .NET. The IAxisRates and IEnumerable interfaces provide
    // this polymorphism. 
    //
    // The Guid attribute sets the CLSID for ASCOM.BrebissonV1.AxisRates
    // The ClassInterface/None attribute prevents an empty interface called
    // _AxisRates from being created and used as the [default] interface
    //
    [Guid("4c852dd8-1e67-4417-9a9b-9dbea3c0207c")]
    [ClassInterface(ClassInterfaceType.None)]
    [ComVisible(true)]
    public class AxisRates : IAxisRates, IEnumerable
    {
        private TelescopeAxes axis;
        private readonly Rate[] rates;
        internal AxisRates(TelescopeAxes axis)
        {
            this.axis = axis;
            switch (axis)
            {
                case TelescopeAxes.axisPrimary:
                    // TODO Initialize this array with any Primary axis rates that your driver may provide
                    // Example: m_Rates = new Rate[] { new Rate(10.5, 30.2), new Rate(54.0, 43.6) }
                    this.rates = new Rate[] { new Rate(1.0/3600.0, SharedResources.raMaxSpeed* 360.0/SharedResources.raMaxPos) };
                    break;
                case TelescopeAxes.axisSecondary:
                    // TODO Initialize this array with any Secondary axis rates that your driver may provide
                    this.rates = new Rate[] { new Rate(1.0/3600.0, SharedResources.decMaxSpeed * 360.0/SharedResources.decMaxPos) };
                    break;
                case TelescopeAxes.axisTertiary:
                    // TODO Initialize this array with any Tertiary axis rates that your driver may provide
                    this.rates = new Rate[0];
                    break;
                default: throw new InvalidValueException("AxisRates on non alpha/dec axes");
            }
        }

        #region IAxisRates Members

        public int Count
        {
            get { return this.rates.Length; }
        }

        public void Dispose()
        {
            // TODO Add any required object clean-up here
        }

        public IEnumerator GetEnumerator()
        {
            return rates.GetEnumerator();
        }

        public IRate this[int index]
        {
            get {
                Debug.Write("get tracking rate " + index.ToString());
                if (index < 1) index = 1;
                return this.rates[index - 1]; }	// 1-based
        }

        #endregion
    }
    #endregion

    #region TrackingRates
    //
    // TrackingRates is a strongly-typed collection that must be enumerable by
    // both COM and .NET. The ITrackingRates and IEnumerable interfaces provide
    // this polymorphism. 
    //
    // The Guid attribute sets the CLSID for ASCOM.BrebissonV1.TrackingRates
    // The ClassInterface/None attribute prevents an empty interface called
    // _TrackingRates from being created and used as the [default] interface
    //
    // This class is implemented in this way so that applications based on .NET 3.5
    // will work with this .NET 4.0 object.  Changes to this have proved to be challenging
    // and it is strongly suggested that it isn't changed.
    //
    [Guid("633f612c-a1c3-4b96-ae56-b8eaf41125fb")]
    [ClassInterface(ClassInterfaceType.None)]
    [ComVisible(true)]
    public class TrackingRates : ITrackingRates, IEnumerable, IEnumerator
    {
        private readonly DriveRates[] trackingRates;
        private readonly ThreadLocal<int> pos = new ThreadLocal<int>(() => { return -1; });
        private static readonly object lockObj = new object();
        internal TrackingRates()
        {
            this.trackingRates = new[] { DriveRates.driveSidereal, DriveRates.driveLunar, DriveRates.driveSolar, DriveRates.driveKing };
        }
        public int Count
        {
            get { return this.trackingRates.Length; }
        }
        public IEnumerator GetEnumerator()
        {
            pos.Value = -1;
            return this as IEnumerator;
        }
        public void Dispose()
        {
            // TODO Add any required object clean-up here
        }
        public DriveRates this[int index]
        {
            get { return this.trackingRates[index - 1]; }   // 1-based
        }
        public object Current
        {
            get
            {
                lock (lockObj)
                {
                    if (pos.Value < 0 || pos.Value >= trackingRates.Length)
                    {
                        throw new System.InvalidOperationException();
                    }
                    return trackingRates[pos.Value];
                }
            }
        }
        public bool MoveNext()
        {
            lock (lockObj)
            {
                if (++pos.Value >= trackingRates.Length)
                {
                    return false;
                }
                return true;
            }
        }
        public void Reset()
        {
            pos.Value = -1;
        }
    }
    #endregion
}
