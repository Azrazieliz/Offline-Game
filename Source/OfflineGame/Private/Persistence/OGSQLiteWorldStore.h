#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

struct sqlite3;

class FOGSQLiteWorldStore final : public IOGWorldStore
{
public:
    FOGSQLiteWorldStore() = default;
    virtual ~FOGSQLiteWorldStore() override;

    virtual bool Open(const FString& AbsoluteDatabasePath, FString& OutError) override;
    virtual void Close() override;
    virtual bool IsOpen() const override { return Database != nullptr; }

    virtual bool BeginTransaction(FString& OutError) override;
    virtual bool CommitTransaction(FString& OutError) override;
    virtual bool RollbackTransaction(FString& OutError) override;

    virtual int32 GetSchemaVersion(FString& OutError) const override;

    virtual bool UpsertEntity(
        const FOGEntityId& EntityId,
        FName Kind,
        int64 CreatedWorldTick,
        const FString& StateJson,
        FString& OutError) override;

    virtual bool TryReadEntity(
        const FOGEntityId& EntityId,
        bool& bOutFound,
        FName& OutKind,
        FString& OutStateJson,
        int64& OutRevision,
        FString& OutError) const override;

    virtual bool UpsertCharacterManifestation(
        const FOGCharacterManifestationRecord& Manifestation,
        int64 CreatedWorldTick,
        FString& OutError) override;

    virtual bool TryReadCharacterManifestation(
        const FOGEntityId& ManifestationId,
        bool& bOutFound,
        FOGCharacterManifestationRecord& OutManifestation,
        FString& OutError) const override;

    virtual bool UpsertContentPackage(
        const FOGContentId& PackageId,
        int32 Version,
        const FString& ContentHash,
        bool bInstalled,
        bool bValidated,
        const FString& ManifestJson,
        FString& OutError) override;

    virtual bool SetContentPackageActivated(
        const FOGContentId& PackageId,
        bool bActivated,
        FString& OutError) override;

    virtual bool IsContentPackageActivated(
        const FOGContentId& PackageId,
        bool& bOutKnown,
        bool& bOutActivated,
        FString& OutError) const override;

    virtual bool AppendWorldEvent(const FOGWorldEvent& Event, FString& OutError) override;

    virtual bool BackupTo(const FString& AbsoluteBackupPath, FString& OutError) override;
    virtual bool RestoreFrom(const FString& AbsoluteBackupPath, FString& OutError) override;
    virtual bool RunIntegrityCheck(FString& OutReport, FString& OutError) const override;
    virtual bool Checkpoint(FString& OutError) override;

    virtual const FString& GetDatabasePath() const override { return DatabasePath; }

private:
    bool ExecuteSql(const FString& Sql, FString& OutError) const;
    bool EnsureMigrationTable(FString& OutError);
    bool ApplyMigrations(FString& OutError);
    bool RecordMigration(int32 Version, const TCHAR* Name, FString& OutError);
    FString LastError(const TCHAR* Context) const;

    sqlite3* Database = nullptr;
    FString DatabasePath;
    bool bTransactionActive = false;
};
