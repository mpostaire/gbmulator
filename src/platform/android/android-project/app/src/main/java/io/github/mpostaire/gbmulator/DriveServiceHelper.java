package io.github.mpostaire.gbmulator;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.api.client.http.FileContent;
import com.google.api.services.drive.Drive;
import com.google.api.services.drive.model.File;
import com.google.api.services.drive.model.FileList;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class DriveServiceHelper {

    private final Executor executor = Executors.newSingleThreadExecutor();

    private Drive driveService;

    public DriveServiceHelper(Drive driveService) {
        this.driveService = driveService;
    }

    public Task<String> createSaveDir() {
        return Tasks.call(executor, () -> {
            File dirMetadata = new File();
            dirMetadata.setName("GBmulator_saves");
            dirMetadata.setMimeType("application/vnd.google-apps.folder");
            File dir = driveService.files().create(dirMetadata)
                    .setFields("id")
                    .execute();
            if (dir == null)
                throw new IOException("Null result when requesting save dir creation");
            return dir.getId();
        });
    }

    public Task<String> uploadSaveFile(String filePath, String parentDirId) {
        return Tasks.call(executor, () -> {
            java.io.File file = new java.io.File(filePath);
            FileContent mediaContent = new FileContent("application/octet-stream", file);
            File fileMetadata = new File();
            fileMetadata.setName(file.getName());
            fileMetadata.setParents(Collections.singletonList(parentDirId));
            File myFile = driveService.files().create(fileMetadata, mediaContent).execute();
            if (myFile == null)
                throw new IOException("Null result when requesting file creation");
            return myFile.getId();
        });
    }

    public Task<ByteArrayOutputStream> downloadSaveFile(String fileId) {
        return Tasks.call(executor, () -> {
            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
            driveService.files().get(fileId).executeMediaAndDownloadTo(byteArrayOutputStream);
            return byteArrayOutputStream;
        });
    }

    public Task<String> updateSaveFile(String localFilePath, String distantFileId) {
        return Tasks.call(executor, () -> {
            java.io.File fileContents = new java.io.File(localFilePath);
            FileContent mediaContent = new FileContent("application/octet-stream", fileContents);

            File newDistantFile = new File();

            File myFile = driveService.files().update(distantFileId, newDistantFile, mediaContent).execute();
            if (myFile == null)
                throw new IOException("Null result when requesting file update");
            return myFile.getId();
        });
    }

    public Task<String> getSaveDirId() {
        return Tasks.call(executor, () -> {
            FileList fileList = driveService.files().list()
                    .setQ("mimeType = 'application/vnd.google-apps.folder' and trashed = false and name = 'GBmulator_saves'")
                    .setSpaces("drive")
                    .setFields("nextPageToken, files(id, name)")
                    .execute();

            if (fileList.isEmpty()) {
                return null;
            }
            return fileList.getFiles().get(0).getId();
        });
    }

    public Task<List<File>> listSaveFiles(String dirId) {
        return Tasks.call(executor, () -> {
            FileList fileList = driveService.files().list()
                    .setQ("'" + dirId +"' in parents and mimeType != 'application/vnd.google-apps.folder' and trashed = false")
                    .setSpaces("drive")
                    .setFields("nextPageToken, files(id, name, parents, modifiedTime)")
                    .execute();

            return fileList.getFiles();
        });
    }
}
