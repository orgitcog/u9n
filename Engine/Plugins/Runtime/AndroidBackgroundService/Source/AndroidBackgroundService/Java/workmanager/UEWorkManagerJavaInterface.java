// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.workmanager;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import androidx.work.BackoffPolicy;
import androidx.work.Data;
import androidx.work.NetworkType;
import androidx.work.Operation;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.WorkRequest;
import androidx.work.OneTimeWorkRequest;
import androidx.work.Constraints;
import androidx.work.ExistingWorkPolicy;
import androidx.work.ExistingPeriodicWorkPolicy;
import androidx.work.Configuration;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import com.google.common.util.concurrent.ListenableFuture;

import com.epicgames.unreal.Logger;
import com.epicgames.unreal.download.datastructs.DownloadQueueDescription;
import com.epicgames.unreal.download.datastructs.DownloadWorkerParameterKeys;
import com.epicgames.unreal.workmanager.UEWorker;


//Helper class to manage our different work requests and callbacks
public class UEWorkManagerJavaInterface
{
	public static Logger Log = new Logger("UE", "UEWorkManagerJavaInterface");

	// list of started task ids; this will be empty on start of application
	private static List<String> startedTaskIDs = new ArrayList<String>();

	public static class FWorkRequestParametersJavaInterface
	{
		public boolean bRequireBatteryNotLow;
		public boolean bRequireCharging;
		public boolean bRequireDeviceIdle;
		public boolean bRequireWifi;
		public boolean bRequireAnyInternet;
		public boolean bAllowRoamingInternet;
		public boolean bRequireStorageNotLow;
		public boolean bStartAsForegroundService;
		public boolean bIsPeriodicWork;
		
		public long InitialStartDelayInSeconds;

		public boolean bIsRecurringWork;
		public int RepeatIntervalInMinutes;

		public boolean bUseLinearBackoffPolicy;
		public int InitialBackoffDelayInSeconds;
		
		public Map<String, Object> ExtraWorkData;
				
		public Class WorkerJavaClass;
		
		public FWorkRequestParametersJavaInterface()
		{
			// WARNING:
			//These defaults are just in here for prosterity, but in reality the defaults in the C++ UEWorkManagerNativeWrapper class
			//are what is actually used since it drives these underlying values (Although the 2 code paths SHOULD match)
			bRequireBatteryNotLow			= false;
			bRequireCharging				= false;
			bRequireDeviceIdle				= false;
			bRequireWifi					= false;
			bRequireAnyInternet				= false;
			bAllowRoamingInternet			= false;
			bRequireStorageNotLow			= false;
			bStartAsForegroundService		= false;
		
			bIsPeriodicWork					= false;
		
			InitialStartDelayInSeconds		= 0;

			bIsRecurringWork				= false;

			//default on the system is 15 min for this, even though we have it turned off want the meaningful default
			RepeatIntervalInMinutes			= 15;		

			//default if not specified is exponential backoff policy with 10s
			bUseLinearBackoffPolicy			= false;
			InitialBackoffDelayInSeconds	= 10;
			
			//by default empty HashMap, but can add to this to have these values end up in the Worker Parameter data.
			ExtraWorkData = new HashMap<String, Object>();
						
			//default to just a generic UEWorker if one isn't supplied
			WorkerJavaClass = UEWorker.class;
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, Object object)
		{
			ExtraWorkData.put(string, object);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, int value)
		{
			ExtraWorkData.put(string, value);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, long value)
		{
			ExtraWorkData.put(string, value);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, float value)
		{
			ExtraWorkData.put(string, value);
		}

		public void AndroidThunk_AddExtraWorkData(String string, double value)
		{
			ExtraWorkData.put(string, value);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, boolean value)
		{
			ExtraWorkData.put(string, value);
		}
	}
	
	public static FWorkRequestParametersJavaInterface AndroidThunkJava_CreateWorkRequestParameters()
	{
		return new FWorkRequestParametersJavaInterface();
	}

	private static boolean isWorkScheduled(WorkManager workManager, String tag)
	{
		ListenableFuture<List<WorkInfo>> workInfos = workManager.getWorkInfosByTag(tag);
		boolean bRunning = false;
		try
		{
			List<WorkInfo> workInfoList = workInfos.get();
			for (WorkInfo workInfo : workInfoList)
			{
				WorkInfo.State state = workInfo.getState();
				bRunning = bRunning || (state == WorkInfo.State.ENQUEUED || state == WorkInfo.State.RUNNING);
			}
			return bRunning;
		}
		catch (Exception e)
		{
		}
		return false;
	}

	public static boolean AndroidThunkJava_RegisterWork(Context AppContext, String TaskID, FWorkRequestParametersJavaInterface InParams)
	{
		boolean bOnlyOneTask = false;

		if (InParams.ExtraWorkData.containsKey(DownloadWorkerParameterKeys.DOWNLOAD_DESCRIPTION_LIST_KEY))
		{
			bOnlyOneTask = true;
			String TaskFilename = (String)InParams.ExtraWorkData.get(DownloadWorkerParameterKeys.DOWNLOAD_DESCRIPTION_LIST_KEY);

			SharedPreferences preferences = AppContext.getSharedPreferences("BackgroundDownloadTasks", android.content.Context.MODE_PRIVATE);
			SharedPreferences.Editor editor = preferences.edit();
			editor.putString("TaskFilename", TaskFilename);
			editor.commit();
		}

		//See all options for constraints at: https://developer.android.com/reference/androidx/work/Constraints.Builder
		Constraints.Builder constraintsBuilder = new Constraints.Builder()
			 .setRequiresBatteryNotLow(InParams.bRequireBatteryNotLow)   
			 .setRequiresCharging(InParams.bRequireCharging)
			 .setRequiresDeviceIdle(InParams.bRequireDeviceIdle)
			 .setRequiresStorageNotLow(InParams.bRequireStorageNotLow);

		if (InParams.bRequireWifi)
		{
			constraintsBuilder.setRequiredNetworkType(NetworkType.UNMETERED);
		}
		else if (InParams.bRequireAnyInternet)
		{
			if (InParams.bAllowRoamingInternet)
			{
				constraintsBuilder.setRequiredNetworkType(NetworkType.CONNECTED);
			}
			else
			{
				constraintsBuilder.setRequiredNetworkType(NetworkType.NOT_ROAMING);
			}
		}
		else
		{
			constraintsBuilder.setRequiredNetworkType(NetworkType.NOT_REQUIRED);
		}
				
		if (InParams.bIsRecurringWork)
		{
			// need to annoyingly duplicate a lot of the below code but using
			//WorkManager.enqueueUniquePeriodicWork(TaskID, ExistingPeriodicWorkPolicy.REPLACE ,newWorkRequest);
			return false;
			
		}
		else
		{	
			BackoffPolicy BackoffPolicyToUse = BackoffPolicy.EXPONENTIAL;
			if (InParams.bUseLinearBackoffPolicy)
			{
				BackoffPolicyToUse = BackoffPolicy.LINEAR;
			}
			
			@SuppressWarnings("unchecked")
			OneTimeWorkRequest newWorkRequest =  new OneTimeWorkRequest.Builder(InParams.WorkerJavaClass)
				.setConstraints(constraintsBuilder.build())
				.addTag(TaskID)
				.setInitialDelay(InParams.InitialStartDelayInSeconds, TimeUnit.SECONDS)
				.setBackoffCriteria(BackoffPolicyToUse, InParams.InitialBackoffDelayInSeconds, TimeUnit.SECONDS)
				.setInputData(
					new Data.Builder()
						.putString("WorkID", TaskID)
						.putAll(InParams.ExtraWorkData)
						.build())
				.build();
			
			boolean bDidQueue = true;
			Operation QueueOperationResult;
			try 
			{
				WorkManager workManager = WorkManager.getInstance(AppContext.getApplicationContext());
				if (bOnlyOneTask)
				{
					// if we haven't enqueued this TaskID for this current execution, we need to replace it
					if (!startedTaskIDs.contains(TaskID))
					{
						// these not needed on first run; REPLACE does seem to work properly
//						workManager.cancelAllWork();
//						workManager.pruneWork();

						startedTaskIDs.add(TaskID);
						Log.debug("Enqueueing first new task: " + TaskID);
						QueueOperationResult = workManager.enqueueUniqueWork(TaskID, ExistingWorkPolicy.REPLACE, newWorkRequest);
					}
					else
					{
						// may already be enqueued or running (only need to enqueue again if not)
						if (isWorkScheduled(workManager, TaskID))
						{
							Log.debug("WorkManager task " + TaskID + " already enqueued or running");
						}
						else
						{
							Log.debug("Enqueueing new task: " + TaskID);
							QueueOperationResult = workManager.enqueueUniqueWork(TaskID, ExistingWorkPolicy.REPLACE, newWorkRequest);
						}
					}
				}
				else
				{
					Log.debug("Enqueueing new task: " + TaskID);
					QueueOperationResult = workManager.enqueueUniqueWork(TaskID, ExistingWorkPolicy.REPLACE, newWorkRequest);
				}
			}
			catch(Exception exp)
			{
				exp.printStackTrace();
				bDidQueue = false;
			}
			
			//TODO TRoss -- consider listening for QueueOperationResult's result so we can flag if it actually queued or not, for now just always assume the queue went through though if we didn't catch
			return bDidQueue;
		}
	}

	public static void AndroidThunkJava_CancelWork(Context AppContext, String TaskID)
	{
		try 
		{
			WorkManager.getInstance(AppContext.getApplicationContext()).cancelUniqueWork(TaskID);
		}
		catch(Exception exp)
		{
			exp.printStackTrace();
		}		
	}
}