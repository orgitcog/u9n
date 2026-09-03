// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using AutomationTool.DeviceReservation;
using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.Data;
using System.Linq;
using UnrealBuildTool;

namespace Gauntlet
{
	/// <summary>
	/// An IDeviceReservationService provides a mechanism to obtain devices from an external service
	/// </summary>
	public interface IDeviceReservationService : IDisposable
	{
		static virtual bool Enabled { get; }
		List<ITargetDevice> ReservedDevices { get; }
		bool CanSupportDeviceConstraint(UnrealDeviceTargetConstraint Constraint);
		bool ReserveDevicesFromService(IEnumerable<UnrealDeviceTargetConstraint> RequestedDevices);
		void ReleaseDevices(IEnumerable<ITargetDevice> Devices);
		void ReportDeviceError(string DeviceName, string ErrorMessage);
	}

	/// <summary>
	/// Horde Service. Enabled by providing the base url via -DeviceURL= and pool via -DevicePool=
	/// </summary>
	public class HordeDeviceReservationService : IDeviceReservationService
	{
		/// <summary>
		/// Whether or not this reservation service is enabled
		/// </summary>
		public static bool Enabled 
			=> CommandUtils.ParseParam(Globals.Params.AllArguments, "DeviceURL") 
			&& CommandUtils.ParseParam(Globals.Params.AllArguments, "DevicePool");

		/// <summary>
		/// List of devices reserved from this service
		/// </summary>
		public List<ITargetDevice> ReservedDevices { get; protected set; } = new List<ITargetDevice>();

		/// <summary>
		/// The base URL of the horde service to request devices from 
		/// </summary>
		[AutoParam("")]
		public string DeviceURL { get; protected set; }

		/// <summary>
		/// Name of the horde device pool to request devices from 
		/// </summary>
		[AutoParamWithNames(Default: "", "DevicePool")]
		public string DevicePoolID { get; protected set; }

		/// <summary>
		/// Endpoint for reservations
		/// </summary>
		private Uri ReservationServerUri;

		/// <summary>
		/// Device to reservation lookup
		/// </summary>
		private Dictionary<ITargetDevice, DeviceReservationAutoRenew> ServiceReservations = new Dictionary<ITargetDevice, DeviceReservationAutoRenew>();

		/// <summary>
		/// Target device info, private for reservation use
		/// </summary>
		private Dictionary<ITargetDevice, DeviceDefinition> ServiceDeviceInfo = new Dictionary<ITargetDevice, DeviceDefinition>();

		private Dictionary<ITargetDevice, bool> InitialConnectionState = new Dictionary<ITargetDevice, bool>();

		private bool? IsInstallStep
		{
			get
			{
				return DevicePool.IsInstallStep;
			}
			set
			{
				DevicePool.IsInstallStep = value;
			}
		}

		public HordeDeviceReservationService()
		{
			AutoParam.ApplyParamsAndDefaults(this, Globals.Params.AllArguments);

			if (!Uri.TryCreate(DeviceURL, UriKind.Absolute, out ReservationServerUri))
			{
				throw new AutomationException("Failed to resolve \"{0}\" as a valid URI", DeviceURL);
			}
		}

		#region IDisposable
		private bool bDisposed;
		~HordeDeviceReservationService()
		{
			Dispose(false);
		}

		public void Dispose()
		{
			Dispose(true);
		}

		public void Dispose(bool bDisposing)
		{
			if (bDisposed)
			{
				return;
			}

			if (bDisposing)
			{
				ReleaseDevices(ReservedDevices);
			}

			bDisposed = true;
			GC.SuppressFinalize(this);
		}
		#endregion

		public virtual bool CanSupportDeviceConstraint(UnrealDeviceTargetConstraint Constraint)
		{
			// By default, do not support desktops. Can be overridden in a subtype
			UnrealTargetPlatform[] SupportedPlatforms = UnrealTargetPlatform.GetValidPlatforms()
				.Where(Platform => !Platform.IsInGroup(UnrealPlatformGroup.Desktop))
				.ToArray();

			if (Constraint.Platform == null || !SupportedPlatforms.Contains(Constraint.Platform.Value))
			{
				// If an unsupported device, we can't reserve it
				Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to reserve service device of type: {Type}", Constraint.Platform);
				return false;
			}

			if (!string.IsNullOrEmpty(Constraint.Model))
			{
				// if specific device model, we can't currently reserve it from (legacy) service
				if (DeviceURL.ToLower().Contains("deviceservice.epicgames.net"))
				{
					Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to reserve service device of model: {Model} on legacy service", Constraint.Model);
					return false;
				}
			}

			return true;
		}

		public virtual bool ReserveDevicesFromService(IEnumerable<UnrealDeviceTargetConstraint> RequestedConstraints)
		{
			// Ensure no duplicate requests of an explicit device
			HashSet<string> DeviceNames = new HashSet<string>();
			foreach (UnrealDeviceTargetConstraint Constraint in RequestedConstraints)
			{
				if (!string.IsNullOrEmpty(Constraint.DeviceName))
				{
					if (DeviceNames.Contains(Constraint.DeviceName))
					{
						Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Attempted to make a reservation for multiple devices using the same device name {DeviceName}. This is not supported.", Constraint.DeviceName);
						return false;
					}

					DeviceNames.Add(Constraint.DeviceName);
				}
			}

			List<ITargetDevice> ScopeReservedDevices = new List<ITargetDevice>();
			foreach (UnrealDeviceTargetConstraint Constraint in RequestedConstraints)
			{
				Reservation NewReservation = Reservation.Create(ReservationServerUri, [Constraint.FormatWithIdentifier()], TimeSpan.FromMinutes(10), RetryMax: 0, PoolID: DevicePoolID, DeviceName: Constraint.DeviceName);
				DeviceReservationAutoRenew DeviceReservation = new DeviceReservationAutoRenew(DeviceURL, NewReservation);

				if (DeviceReservation == null || DeviceReservation.Devices.Count != 1)
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to make device registration with constraint {Constraint}", Constraint);
					return false;
				}

				if (DevicePool.DeviceReservationBlock = DeviceReservation.InstallRequired != null)
				{
					// InstallRequired is true only for the first reservation attempt of the current step. Cache that value to avoid conflicting value on retry attempt.
					IsInstallStep = IsInstallStep != null ? IsInstallStep : DeviceReservation.InstallRequired == true;
					DevicePool.SkipInstall = DeviceReservation.InstallRequired == false && IsInstallStep == false;
					DevicePool.FullClean = !DevicePool.SkipInstall;
				}

				// Construct a definition from the reservation
				Device Device = DeviceReservation.Devices[0];
				DeviceDefinition DeviceDefinition = new DeviceDefinition()
				{
					Address = Device.IPOrHostName,
					Name = Device.Name,
					Platform = UnrealTargetPlatform.Parse(UnrealTargetPlatform.GetValidPlatformNames().FirstOrDefault(Entry => Entry == Device.Type.Replace("-DevKit", "", StringComparison.OrdinalIgnoreCase))),
					DeviceData = Device.DeviceData,
					Model = Device.Model
				};

				EPerfSpec PerfSpec = EPerfSpec.Unspecified;
				if (!string.IsNullOrEmpty(Device.PerfSpec) && !Enum.TryParse(Device.PerfSpec, true, out PerfSpec))
				{
					throw new AutomationException("Unable to convert perfspec '{0}' into an EPerfSpec", Device.PerfSpec);
				}
				DeviceDefinition.PerfSpec = PerfSpec;

				ITargetDevice TargetDevice = DevicePool.Instance.CreateAndRegisterDeviceFromDefinition(DeviceDefinition, Constraint, this);

				// If a device from service can't be added, fail reservation and cleanup devices
				if (TargetDevice == null)
				{
					// If some devices from reservation have been created, release them which will also dispose of reservation
					if (ScopeReservedDevices.Count > 0)
					{
						ReleaseDevices(ScopeReservedDevices);
					}

					// Cancel this reservation
					DeviceReservation.Dispose();

					Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to make device registration: device registration failed for {Platform}:{DeviceName}", DeviceDefinition.Platform, DeviceDefinition.Name);
					return false;
				}
				else
				{
					ScopeReservedDevices.Add(TargetDevice);
					ReservedDevices.Add(TargetDevice);
					ServiceDeviceInfo.Add(TargetDevice, DeviceDefinition);
					ServiceReservations.Add(TargetDevice, DeviceReservation);
					InitialConnectionState.Add(TargetDevice, TargetDevice.IsConnected);
				}
			}

			if (ScopeReservedDevices.Count == RequestedConstraints.Count())
			{
				return true;
			}

			Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to reserve all devices from service.");
			return false;
		}

		/// <summary>
		/// Release all devices in the provided list from our reserved list
		/// </summary>
		/// <param name="DeviceList"></param>
		public void ReleaseDevices(IEnumerable<ITargetDevice> DeviceList)
		{
			if (!DeviceList.Any())
			{
				return;
			}

			// Remove all these devices from our reserved list
			ReservedDevices = ReservedDevices.Except(DeviceList).ToList();

			List<ITargetDevice> ThrowDevices = new List<ITargetDevice>();
			foreach (ITargetDevice Device in DeviceList)
			{
				// Reset any state if necessary
				DeviceConfigurationCache.Instance.RevertDeviceConfiguration(Device);

				if (Device.IsConnected && !InitialConnectionState[Device])
				{
					Device.Disconnect();
				}

				// Unregister device
				if (ServiceReservations.TryGetValue(Device, out DeviceReservationAutoRenew Reservation))
				{
					bool DisposeReservation = ServiceReservations.Count(Entry => Entry.Value == Reservation) == 1;

					// remove and dispose of device
					// @todo: add support for reservation modification on server (partial device release)
					ServiceReservations.Remove(Device);
					ServiceDeviceInfo.Remove(Device);
					InitialConnectionState.Remove(Device);

					Device.Dispose();
					Reservation.Dispose();
				}
				else
				{
					ThrowDevices.Add(Device);
				}
			}

			if (ThrowDevices.Any())
			{
				// If a user explicitly calls a service's release function with an incorrect device, throw this exception
				string ExceptionMessage = "Attempted to release the following devices from a service that did not reserve them!";
				ExceptionMessage += "\n\t" + string.Join("\n\t", ThrowDevices.Select(Device => Device.Name));
				ExceptionMessage += "\nUse DevicePool.Instance.ReleaseDevices to avoid mistakingly releasing devices reserved from a different service";
				throw new AutomationException(ExceptionMessage);
			}
		}

		/// <summary>
		/// Report target device issue to service with given error message
		/// </summary>
		public virtual void ReportDeviceError(string DeviceName, string ErrorMessage)
		{
			// TargetDevice name is not always DeviceData name... need to try to resolve target device names
			ITargetDevice MatchingDevice = null;
			foreach (ITargetDevice Device in ReservedDevices)
			{
				if (Device.Name.Equals(DeviceName, StringComparison.OrdinalIgnoreCase))
				{
					MatchingDevice = Device;
					break;
				}
			}

			if (MatchingDevice == null)
			{
				// No Target device, assume this is DeviceData name
				Reservation.ReportDeviceError(DeviceURL, DeviceName, ErrorMessage);
			}
			else
			{
				Reservation.ReportDeviceError(DeviceURL, ServiceDeviceInfo[MatchingDevice].Name, ErrorMessage);
			}
		}
	}
}