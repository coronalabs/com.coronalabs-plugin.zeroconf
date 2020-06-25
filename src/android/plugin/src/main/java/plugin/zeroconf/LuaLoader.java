//
//  LuaLoader.java
//  TemplateApp
//
//  Copyright (c) 2012 Corona Labs. All rights reserved.
//

// This corresponds to the name of the Lua library,
// e.g. [Lua] require "plugin.library"
package plugin.zeroconf;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

import com.ansca.corona.CoronaEnvironment;
import com.ansca.corona.CoronaLua;
import com.ansca.corona.CoronaRuntime;
import com.ansca.corona.CoronaRuntimeTask;
import com.ansca.corona.CoronaRuntimeTaskDispatcher;
import com.naef.jnlua.JavaFunction;
import com.naef.jnlua.LuaState;
import com.naef.jnlua.LuaType;
import com.naef.jnlua.NamedJavaFunction;

import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;


/**
 * Implements the Lua interface for a Corona plugin.
 * <p>
 * Only one instance of this class will be created by Corona for the lifetime of the application.
 * This instance will be re-used for every new Corona activity that gets created.
 */
@SuppressWarnings({"unused"})
public class LuaLoader implements JavaFunction {
	/** Lua registry ID to the Lua function to be called when the ad request finishes. */
	private int fListener;

	/** This corresponds to the event name, e.g. [Lua] event.name */
	private static final String EVENT_NAME = "PluginZeroConfEvent";
	private static final String DEFAULT_TYPE = "_corona._tcp";

    private static Integer REGISTER_KEY = 1984;

    private Map<Integer, CoronaRegistrationListener> publishers = new HashMap<>();
    private Map<Integer, CoronaBrowserListener> browsers = new HashMap<>();

	/**
	 * Creates a new Lua interface to this plugin.
	 * <p>
	 * Note that a new LuaLoader instance will not be created for every CoronaActivity instance.
	 * That is, only one instance of this class will be created for the lifetime of the application process.
	 * This gives a plugin the option to do operations in the background while the CoronaActivity is destroyed.
	 */
	@SuppressWarnings("unused")
	public LuaLoader() {
		// Initialize member variables.
		fListener = CoronaLua.REFNIL;
    }

	/**
	 * Called when this plugin is being loaded via the Lua require() function.
	 * <p>
	 * Note that this method will be called every time a new CoronaActivity has been launched.
	 * This means that you'll need to re-initialize this plugin here.
	 * <p>
	 * Warning! This method is not called on the main UI thread.
	 * @param L Reference to the Lua state that the require() function was called from.
	 * @return Returns the number of values that the require() function will return.
	 *         <p>
	 *         Expected to return 1, the zecoronf that the require() function is loading.
	 */
	@Override
	public int invoke(LuaState L) {
		// Register this plugin into Lua with the following functions.
		NamedJavaFunction[] luaFunctions;
        if (android.os.Build.VERSION.SDK_INT >= 16) {
            luaFunctions = new NamedJavaFunction[]{
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "init";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return init(luaState);
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "publish";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return publish(luaState);
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "unpublish";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return unpublish(luaState);
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "browse";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return browse(luaState);
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "stopBrowse";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return stopBrowse(luaState);
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "unpublishAll";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return unpublishAll(luaState);
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "stopBrowseAll";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return stopBrowseAll(luaState);
                        }
                    }
            };
        }
        else
        {
            luaFunctions = new NamedJavaFunction[]{
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "init";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "publish";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "unpublish";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "browse";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "stopBrowse";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "unpublishAll";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    },
                    new NamedJavaFunction() {
                        @Override
                        public String getName() {
                            return "stopBrowseAll";
                        }

                        @Override
                        public int invoke(LuaState luaState) {
                            return 0;
                        }
                    }
            };
        }

		String libName = L.toString( 1 );
		L.register(libName, luaFunctions);

		// Returning 1 indicates that the Lua require() function will return the above Lua zecoronf.
		return 1;
	}

    NsdManager fNsdManager;
    CoronaRuntimeTaskDispatcher fDispatcher;

    @TargetApi(16)
    public void DispatchServiceEvent(final String phase, final NsdServiceInfo service, final int errorCode, final Integer browser, final Integer publisher)
    {
        fDispatcher.send(new CoronaRuntimeTask() {
            @Override
            public void executeUsing(CoronaRuntime coronaRuntime) {
                LuaState L = coronaRuntime.getLuaState();
                CoronaLua.newEvent(L, EVENT_NAME);

                L.pushString(phase);
                L.setField(-2, "phase");

                if (errorCode!=0)
                {
                    L.pushInteger(errorCode);
                    L.setField(-2, "errorCode");
                }

                L.pushBoolean(errorCode!=0);
                L.setField(-2, "isError");

                if(browser!=null)
                {
                    L.pushInteger(browser);
                    L.setField(-2, "browser");
                }

                if(publisher!=null)
                {
                    L.pushInteger(publisher);
                    L.setField(-2, "publisher");
                }

                if(service!=null)
                {
                    L.pushJavaObject(service);
                    L.setField(-2, "service");

                    if(service.getServiceName()!=null)
                    {
                        L.pushString(service.getServiceName());
                        L.setField(-2, "serviceName");
                    }

                    if(service.getServiceType()!=null)
                    {
                        L.pushString(service.getServiceType());
                        L.setField(-2, "type");
                    }

                    if(service.getPort() != -1)
                    {
                        L.pushInteger(service.getPort());
                        L.setField(-2, "port");
                    }

                    Map<String, byte[]> attrs = null;
                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                        attrs = service.getAttributes();
                    }
                    if (attrs!=null)
                    {
                        L.newTable(0, attrs.size());
                        for (Map.Entry<String, byte[]> e : attrs.entrySet()) {
                            L.pushString(e.getValue());
                            L.setField(-2, e.getKey());
                        }
                        L.setField(-2, "data");
                    }

                    InetAddress address = service.getHost();
                    if(address!=null) {
                        //address.getHostName() is blocking DNS lookup operation! Taking really long time sometimes
//                        String host = address.getHostName();
//                        if (host!=null) {
//                            L.pushString(host);
//                            L.setField(-2, "hostname");
//                        }

                        String ip = address.getHostAddress();
                        if(ip!=null) {
                            L.newTable(1, 0);
                            L.pushString(ip);
                            L.rawSet(-2, 1);

                            L.setField(-2, "addresses");
                        }
                    }

                }

                try {
                    CoronaLua.dispatchEvent(L, fListener, 0);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }

    @TargetApi(16)
    class CoronaRegistrationListener implements NsdManager.RegistrationListener {

        private final Integer publisher;

        public CoronaRegistrationListener(Integer key) {
            this.publisher = key;
        }

        @Override
        public void onRegistrationFailed(NsdServiceInfo nsdServiceInfo, int i) {
            DispatchServiceEvent("published", nsdServiceInfo, i, null, publisher);
        }

        @Override
        public void onUnregistrationFailed(NsdServiceInfo nsdServiceInfo, int i) {
            publishers.remove(publisher);
        }

        @Override
        public void onServiceRegistered(NsdServiceInfo nsdServiceInfo) {
            DispatchServiceEvent("published", nsdServiceInfo, 0, null, publisher);
        }

        @Override
        public void onServiceUnregistered(NsdServiceInfo nsdServiceInfo) {
            publishers.remove(publisher);
        }
    }

    @TargetApi(16)
    class CoronaBrowserListener implements NsdManager.DiscoveryListener {
        private final Integer browser;

        public CoronaBrowserListener(Integer key) {
            this.browser = key;
        }

        @Override
        public void onStartDiscoveryFailed(String s, int i) {
            DispatchServiceEvent("browseError", null, i, browser, null);
        }

        @Override
        public void onStopDiscoveryFailed(String s, int i) {
            browsers.remove(browser);
        }

        @Override
        public void onDiscoveryStarted(String s) {
        }

        @Override
        public void onDiscoveryStopped(String s) {
            browsers.remove(browser);
        }

        @Override
        public void onServiceFound(NsdServiceInfo nsdServiceInfo) {
            fNsdManager.resolveService(nsdServiceInfo, new CoronaResolveListener(browser));
        }

        @Override
        public void onServiceLost(NsdServiceInfo nsdServiceInfo) {
            DispatchServiceEvent("lost", nsdServiceInfo, 0, browser, null);
        }
    }

    @TargetApi(16)
    class  CoronaResolveListener implements NsdManager.ResolveListener {
        private final Integer browser;

        public CoronaResolveListener(Integer key) {
            this.browser = key;
        }

        @Override
        public void onResolveFailed(NsdServiceInfo nsdServiceInfo, int i) {
        }

        @Override
        public void onServiceResolved(NsdServiceInfo nsdServiceInfo) {
            DispatchServiceEvent("found", nsdServiceInfo, 0, browser, null);
        }
    }

    @TargetApi(16)
    private int browse(LuaState L) {

        final int idx = 1;
        String type = DEFAULT_TYPE;

        if(L.isTable(idx)) {
            L.getField(idx, "type");
            if (L.type(-1) == LuaType.STRING)
            {
                type = L.toString(-1);
            }
            L.pop(1);
        }

        if (fNsdManager == null)
        {
            fNsdManager = (NsdManager) CoronaEnvironment.getApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        if (fDispatcher == null)
        {
            fDispatcher = new CoronaRuntimeTaskDispatcher(L);
        }

        Integer key = REGISTER_KEY;
        REGISTER_KEY = REGISTER_KEY+1;

        CoronaBrowserListener listener = new CoronaBrowserListener(key);
        fNsdManager.discoverServices(type, NsdManager.PROTOCOL_DNS_SD, listener);

        browsers.put(key, listener);

        L.pushInteger(key);
        return 1;
    }

    @TargetApi(16)
    private int stopBrowse(LuaState L) {
        if (fNsdManager == null)
        {
            fNsdManager = (NsdManager) CoronaEnvironment.getApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        try {
            final int idx = 1;
            if(L.type(idx) == LuaType.NUMBER) {
                Integer key = L.toInteger(idx);
                CoronaBrowserListener listener = browsers.remove(key);
                if(listener!=null)
                {
                    fNsdManager.stopServiceDiscovery(listener);
                }
                else
                {
                    Log.w("Corona", "zeroconf.stopBrowse(): unable to find such browser!");
                }
            }
            else
            {
                Log.w("Corona", "zeroconf.stopBrowse(): did not receive service browser as first parameter");
            }
        }
        catch (Exception e)
        {
            Log.w("Corona", "zeroconf.stopBrowse(): error while stopping browser!");
        }
        return 0;
    }

    @TargetApi(16)
    private int publish(LuaState L) {
        final int idx = 1;

        NsdServiceInfo service = new NsdServiceInfo();
        if (L.isTable(idx))
        {
            L.getField(idx, "port");
            if(L.type(-1) == LuaType.NUMBER)
            {
                service.setPort(L.toInteger(-1));
            }
            else
            {
                Log.e("Corona", "zeroconf.publish(): parameters table does not contain 'port' field.");
                L.pushNil();
                return 1;
            }
            L.pop(1);

            L.getField(idx, "name");
            if(L.type(-1) == LuaType.STRING)
            {
                service.setServiceName(L.toString(-1));
            }
            else
            {
                service.setServiceName(android.os.Build.MODEL);
            }
            L.pop(1);

            L.getField(idx, "serviceName");
            if(L.type(-1) == LuaType.STRING)
            {
                service.setServiceName(L.toString(-1));
            }
            L.pop(1);

            L.getField(idx, "type");
            if(L.type(-1) == LuaType.STRING)
            {
                service.setServiceType(L.toString(-1));
            }
            else
            {
                service.setServiceType(DEFAULT_TYPE);
            }
            L.pop(1);

            L.getField(idx, "data");
            if(L.type(-1) == LuaType.TABLE)
            {
                if (android.os.Build.VERSION.SDK_INT >= 21) {
                    L.pushNil();
                    while (L.next(-2)) {
                        if (L.type(-2) == LuaType.STRING && L.isString(-1))
                            service.setAttribute(L.toString(-2), L.toString(-1));
                        L.pop(1);
                    }
                }
            }
            L.pop(1);
        }
        else
        {
            Log.e("Corona", "zeroconf.publish(): did not receive parameters table");
            L.pushNil();
            return 1;
        }

        if (fNsdManager == null)
        {
            fNsdManager = (NsdManager) CoronaEnvironment.getApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        if (fDispatcher == null)
        {
            fDispatcher = new CoronaRuntimeTaskDispatcher(L);
        }

        Integer key = REGISTER_KEY;
        REGISTER_KEY = REGISTER_KEY+1;

        CoronaRegistrationListener listener = new CoronaRegistrationListener(key);

        fNsdManager.registerService(service, NsdManager.PROTOCOL_DNS_SD, listener);

        publishers.put(key, listener);

        L.pushInteger(key);

        return 1;
    }

	public int init(LuaState L) {
		int listenerIndex = 1;

        if(fListener != CoronaLua.REFNIL) {
            CoronaLua.deleteRef(L, fListener);
        }

		if ( CoronaLua.isListener( L, listenerIndex, EVENT_NAME ) ) {
			fListener = CoronaLua.newRef( L, listenerIndex );
		}

		return 0;
	}

    @TargetApi(16)
    private int unpublish(LuaState L) {
        if (fNsdManager == null)
        {
            fNsdManager = (NsdManager) CoronaEnvironment.getApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        try {
            final int idx = 1;
            if(L.type(idx) == LuaType.NUMBER) {
                Integer key = L.toInteger(idx);
                CoronaRegistrationListener listener = publishers.remove(key);
                if(listener!=null)
                {
                    fNsdManager.unregisterService(listener);
                }
                else
                {
                    Log.w("Corona", "zeroconf.unpublish(): unable to find such service!");
                }
            }
            else
            {
                Log.w("Corona", "zeroconf.unpublish(): did not receive service publisher as first parameter");
            }
        }
        catch (Exception e)
        {
            Log.w("Corona", "zeroconf.unpublish(): error while stopping service!");
        }
        return 0;
    }

    @TargetApi(16)
    private int unpublishAll(LuaState L) {
        if (fNsdManager == null)
        {
            fNsdManager = (NsdManager) CoronaEnvironment.getApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        for (Map.Entry<Integer, CoronaRegistrationListener> e: publishers.entrySet()) {
            try {
                fNsdManager.unregisterService(e.getValue());
            }
            catch (Exception ignore) {
                Log.i("Corona", "zeroconf.unpublishAll(): skipping dead publisher");
            }
        }

        publishers.clear();

        return 0;
    }

    @TargetApi(16)
    private int stopBrowseAll(LuaState L) {
        if (fNsdManager == null)
        {
            fNsdManager = (NsdManager) CoronaEnvironment.getApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        for (Map.Entry<Integer, CoronaBrowserListener> e: browsers.entrySet()) {
            try {
                fNsdManager.stopServiceDiscovery(e.getValue());
            }
            catch (Exception ignore) {
                Log.i("Corona", "zeroconf.stopBrowseAll(): skipping dead browser");
            }
        }

        browsers.clear();

        return 0;
    }

}
