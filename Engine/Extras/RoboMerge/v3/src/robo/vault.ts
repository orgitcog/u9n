// Copyright Epic Games, Inc. All Rights Reserved.

import * as fs from 'fs';
import { ContextualLogger } from '../common/logger';

export class Vault {

    private _vault: any
    readonly valid: boolean = false

    constructor(vaultPath: string, vaultLogger?: ContextualLogger) {

        if (!vaultLogger) {
            vaultLogger = new ContextualLogger('Vault Init')
        }

        try {
            var vaultString = fs.readFileSync(vaultPath + '/vault.json', 'ascii')
        }
        catch (err) {
            const errStr = err.toString().replace(/(E|e)(R|r)(R|r)(O|o)(R|r)/,"$1$2$30$5")
            vaultLogger.warn(`Warning, failed to read vault (ok in dev): ${errStr}`)
            return
        }
        
        try {
            this._vault = JSON.parse(vaultString)
        }
        catch (err) {
            vaultLogger.error(`Failed to parse vault file: ${err}`)
            return
        }

        this.valid = true
    }

    get slackTokens() {
        return this._vault && this._vault['slack-tokens']
    }

    get cookieKey() {
        return this._vault && this._vault['cookie-key']
    }

    get ldapPassword() {
        return this._vault && this._vault['ldap-password']
    }
    
    get perforceServers() { 
        const servers = this._vault && this._vault['servers']
        return servers ? new Map(Object.entries(servers)) : null
    }
}